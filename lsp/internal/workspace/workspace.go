// Package workspace manages compiler invocation and diagnostic conversion.
package workspace

import (
	"bytes"
	"context"
	"encoding/json"
	"fmt"
	"net/url"
	"os"
	"os/exec"
	"path/filepath"
	"strings"
	"sync"
	"time"

	"github.com/aria-lang/aria-lsp/internal/document"
	ariaprot "github.com/aria-lang/aria-lsp/internal/protocol"
)

type Workspace struct {
	docs         *document.Store
	compilerPath string
	rootURI      string

	mu         sync.Mutex
	debounceT  *time.Timer
	pendingURI string

	publishFn func(uri string, diagnostics []LSPDiagnostic)
}

type LSPDiagnostic struct {
	Range    LSPRange `json:"range"`
	Severity int      `json:"severity"`
	Code     string   `json:"code"`
	Source   string   `json:"source"`
	Message  string   `json:"message"`
}

type LSPRange struct {
	Start LSPPosition `json:"start"`
	End   LSPPosition `json:"end"`
}

type LSPPosition struct {
	Line      int `json:"line"`
	Character int `json:"character"`
}

func New(docs *document.Store, compilerPath, rootURI string, publishFn func(string, []LSPDiagnostic)) *Workspace {
	return &Workspace{
		docs:         docs,
		compilerPath: compilerPath,
		rootURI:      rootURI,
		publishFn:    publishFn,
	}
}

func (w *Workspace) OnChange(uri string) {
	w.mu.Lock()
	defer w.mu.Unlock()
	w.pendingURI = uri
	if w.debounceT != nil {
		w.debounceT.Stop()
	}
	w.debounceT = time.AfterFunc(300*time.Millisecond, func() {
		w.check(uri)
	})
}

func (w *Workspace) CheckNow(uri string) {
	w.check(uri)
}

func (w *Workspace) check(uri string) {
	doc, ok := w.docs.Get(uri)
	if !ok {
		return
	}

	filePath := uriToPath(uri)

	// Write overlay to temp file for the compiler
	tmpDir, err := os.MkdirTemp("", "aria-lsp-*")
	if err != nil {
		return
	}
	defer os.RemoveAll(tmpDir)

	tmpFile := filepath.Join(tmpDir, filepath.Base(filePath))
	if err := os.WriteFile(tmpFile, []byte(doc.Content), 0644); err != nil {
		return
	}

	ctx, cancel := context.WithTimeout(context.Background(), 10*time.Second)
	defer cancel()

	cmd := exec.CommandContext(ctx, w.compilerPath, "check", "--format=json", tmpFile)
	var stderr bytes.Buffer
	var stdout bytes.Buffer
	cmd.Stderr = &stderr
	cmd.Stdout = &stdout

	_ = cmd.Run() // exit code != 0 is expected for files with errors

	// Diagnostics may come from stderr (errors) or stdout (success with empty list)
	var output ariaprot.DiagnosticOutput
	jsonData := stderr.Bytes()
	if len(jsonData) == 0 {
		jsonData = stdout.Bytes()
	}
	if len(jsonData) > 0 {
		if err := json.Unmarshal(jsonData, &output); err != nil {
			// If JSON parse fails, try the other stream
			jsonData = stdout.Bytes()
			if len(jsonData) > 0 {
				_ = json.Unmarshal(jsonData, &output)
			}
		}
	}

	lspDiags := make([]LSPDiagnostic, 0, len(output.Diagnostics))
	for _, d := range output.Diagnostics {
		lspDiags = append(lspDiags, convertDiagnostic(d))
	}

	w.publishFn(uri, lspDiags)
}

func convertDiagnostic(d ariaprot.Diagnostic) LSPDiagnostic {
	severity := 1 // Error
	switch d.Severity {
	case "error":
		severity = 1
	case "warning":
		severity = 2
	case "info":
		severity = 3
	case "hint":
		severity = 4
	}

	line := d.Line - 1
	if line < 0 {
		line = 0
	}
	col := d.Column - 1
	if col < 0 {
		col = 0
	}

	endCol := col
	if d.Span[1] > d.Span[0] {
		endCol = col + (d.Span[1] - d.Span[0])
	} else {
		endCol = col + 1
	}

	msg := d.Message
	if len(d.Notes) > 0 {
		msg += "\n" + strings.Join(d.Notes, "\n")
	}

	return LSPDiagnostic{
		Range: LSPRange{
			Start: LSPPosition{Line: line, Character: col},
			End:   LSPPosition{Line: line, Character: endCol},
		},
		Severity: severity,
		Code:     d.Code,
		Source:   "aria",
		Message:  msg,
	}
}

func uriToPath(uri string) string {
	u, err := url.Parse(uri)
	if err != nil {
		return strings.TrimPrefix(uri, "file://")
	}
	return u.Path
}

func URIFromPath(path string) string {
	abs, err := filepath.Abs(path)
	if err != nil {
		abs = path
	}
	return fmt.Sprintf("file://%s", abs)
}

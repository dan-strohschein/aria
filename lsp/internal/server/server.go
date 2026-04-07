// Package server implements the Aria LSP server using JSON-RPC over stdio.
package server

import (
	"bufio"
	"encoding/json"
	"fmt"
	"io"
	"log"
	"os"
	"strconv"
	"strings"
	"sync"

	"github.com/aria-lang/aria-lsp/internal/document"
	"github.com/aria-lang/aria-lsp/internal/symbols"
	"github.com/aria-lang/aria-lsp/internal/workspace"
)

type Server struct {
	reader *bufio.Reader
	writer io.Writer
	mu     sync.Mutex

	docs      *document.Store
	workspace *workspace.Workspace
	rootURI   string

	compilerPath string
	initialized  bool
	shutdown     bool
}

func New(compilerPath string) *Server {
	s := &Server{
		reader:       bufio.NewReader(os.Stdin),
		writer:       os.Stdout,
		docs:         document.NewStore(),
		compilerPath: compilerPath,
	}
	return s
}

func (s *Server) Run() error {
	log.SetOutput(os.Stderr)
	log.Println("aria-lsp: starting")

	for {
		msg, err := s.readMessage()
		if err != nil {
			if err == io.EOF {
				return nil
			}
			return fmt.Errorf("read message: %w", err)
		}

		s.handleMessage(msg)

		if s.shutdown {
			return nil
		}
	}
}

// JSON-RPC message types

type jsonrpcMessage struct {
	JSONRPC string          `json:"jsonrpc"`
	ID      json.RawMessage `json:"id,omitempty"`
	Method  string          `json:"method"`
	Params  json.RawMessage `json:"params,omitempty"`
}

type jsonrpcResponse struct {
	JSONRPC string      `json:"jsonrpc"`
	ID      interface{} `json:"id"`
	Result  interface{} `json:"result,omitempty"`
	Error   *rpcError   `json:"error,omitempty"`
}

type rpcError struct {
	Code    int    `json:"code"`
	Message string `json:"message"`
}

func (s *Server) readMessage() (*jsonrpcMessage, error) {
	var contentLength int

	for {
		line, err := s.reader.ReadString('\n')
		if err != nil {
			return nil, err
		}
		line = strings.TrimSpace(line)
		if line == "" {
			break
		}
		if strings.HasPrefix(line, "Content-Length:") {
			val := strings.TrimSpace(strings.TrimPrefix(line, "Content-Length:"))
			contentLength, _ = strconv.Atoi(val)
		}
	}

	if contentLength == 0 {
		return nil, fmt.Errorf("missing Content-Length header")
	}

	body := make([]byte, contentLength)
	if _, err := io.ReadFull(s.reader, body); err != nil {
		return nil, err
	}

	var msg jsonrpcMessage
	if err := json.Unmarshal(body, &msg); err != nil {
		return nil, err
	}
	return &msg, nil
}

func (s *Server) sendResponse(id json.RawMessage, result interface{}) {
	resp := jsonrpcResponse{
		JSONRPC: "2.0",
		ID:      json.RawMessage(id),
		Result:  result,
	}
	s.sendJSON(resp)
}

func (s *Server) sendError(id json.RawMessage, code int, message string) {
	resp := jsonrpcResponse{
		JSONRPC: "2.0",
		ID:      json.RawMessage(id),
		Error:   &rpcError{Code: code, Message: message},
	}
	s.sendJSON(resp)
}

func (s *Server) sendNotification(method string, params interface{}) {
	msg := struct {
		JSONRPC string      `json:"jsonrpc"`
		Method  string      `json:"method"`
		Params  interface{} `json:"params"`
	}{
		JSONRPC: "2.0",
		Method:  method,
		Params:  params,
	}
	s.sendJSON(msg)
}

func (s *Server) sendJSON(v interface{}) {
	s.mu.Lock()
	defer s.mu.Unlock()

	data, err := json.Marshal(v)
	if err != nil {
		log.Printf("marshal error: %v", err)
		return
	}
	header := fmt.Sprintf("Content-Length: %d\r\n\r\n", len(data))
	s.writer.Write([]byte(header))
	s.writer.Write(data)
}

func (s *Server) handleMessage(msg *jsonrpcMessage) {
	switch msg.Method {
	case "initialize":
		s.handleInitialize(msg)
	case "initialized":
		// no-op
	case "shutdown":
		s.handleShutdown(msg)
	case "exit":
		s.shutdown = true
	case "textDocument/didOpen":
		s.handleDidOpen(msg)
	case "textDocument/didChange":
		s.handleDidChange(msg)
	case "textDocument/didClose":
		s.handleDidClose(msg)
	case "textDocument/didSave":
		s.handleDidSave(msg)
	case "textDocument/documentSymbol":
		s.handleDocumentSymbol(msg)
	default:
		if msg.ID != nil && len(msg.ID) > 0 && string(msg.ID) != "null" {
			s.sendError(msg.ID, -32601, "method not found: "+msg.Method)
		}
	}
}

// Initialize

type initializeParams struct {
	RootURI string `json:"rootUri"`
}

func (s *Server) handleInitialize(msg *jsonrpcMessage) {
	var params initializeParams
	json.Unmarshal(msg.Params, &params)
	s.rootURI = params.RootURI

	s.workspace = workspace.New(s.docs, s.compilerPath, s.rootURI, s.publishDiagnostics)
	s.initialized = true

	result := map[string]interface{}{
		"capabilities": map[string]interface{}{
			"textDocumentSync": map[string]interface{}{
				"openClose": true,
				"change":    1, // Full sync
				"save":      map[string]interface{}{"includeText": true},
			},
			"documentSymbolProvider": true,
		},
		"serverInfo": map[string]interface{}{
			"name":    "aria-lsp",
			"version": "0.1.0",
		},
	}

	s.sendResponse(msg.ID, result)
}

func (s *Server) handleShutdown(msg *jsonrpcMessage) {
	s.sendResponse(msg.ID, nil)
	s.shutdown = true
}

// Document sync

type didOpenParams struct {
	TextDocument struct {
		URI        string `json:"uri"`
		LanguageID string `json:"languageId"`
		Version    int32  `json:"version"`
		Text       string `json:"text"`
	} `json:"textDocument"`
}

type didChangeParams struct {
	TextDocument struct {
		URI     string `json:"uri"`
		Version int32  `json:"version"`
	} `json:"textDocument"`
	ContentChanges []struct {
		Text string `json:"text"`
	} `json:"contentChanges"`
}

type didCloseParams struct {
	TextDocument struct {
		URI string `json:"uri"`
	} `json:"textDocument"`
}

type didSaveParams struct {
	TextDocument struct {
		URI string `json:"uri"`
	} `json:"textDocument"`
	Text string `json:"text,omitempty"`
}

func (s *Server) handleDidOpen(msg *jsonrpcMessage) {
	var params didOpenParams
	json.Unmarshal(msg.Params, &params)

	s.docs.Open(params.TextDocument.URI, params.TextDocument.Text, params.TextDocument.Version)
	s.workspace.CheckNow(params.TextDocument.URI)
}

func (s *Server) handleDidChange(msg *jsonrpcMessage) {
	var params didChangeParams
	json.Unmarshal(msg.Params, &params)

	if len(params.ContentChanges) > 0 {
		s.docs.Update(params.TextDocument.URI, params.ContentChanges[len(params.ContentChanges)-1].Text, params.TextDocument.Version)
		s.workspace.OnChange(params.TextDocument.URI)
	}
}

func (s *Server) handleDidClose(msg *jsonrpcMessage) {
	var params didCloseParams
	json.Unmarshal(msg.Params, &params)

	s.docs.Close(params.TextDocument.URI)
	// Clear diagnostics for closed file
	s.publishDiagnostics(params.TextDocument.URI, nil)
}

func (s *Server) handleDidSave(msg *jsonrpcMessage) {
	var params didSaveParams
	json.Unmarshal(msg.Params, &params)

	if params.Text != "" {
		s.docs.Update(params.TextDocument.URI, params.Text, 0)
	}
	s.workspace.CheckNow(params.TextDocument.URI)
}

// Document symbols

type documentSymbolParams struct {
	TextDocument struct {
		URI string `json:"uri"`
	} `json:"textDocument"`
}

type documentSymbol struct {
	Name           string           `json:"name"`
	Detail         string           `json:"detail,omitempty"`
	Kind           int              `json:"kind"`
	Range          lspRange         `json:"range"`
	SelectionRange lspRange         `json:"selectionRange"`
	Children       []documentSymbol `json:"children,omitempty"`
}

type lspRange struct {
	Start lspPosition `json:"start"`
	End   lspPosition `json:"end"`
}

type lspPosition struct {
	Line      int `json:"line"`
	Character int `json:"character"`
}

func (s *Server) handleDocumentSymbol(msg *jsonrpcMessage) {
	var params documentSymbolParams
	json.Unmarshal(msg.Params, &params)

	doc, ok := s.docs.Get(params.TextDocument.URI)
	if !ok {
		s.sendResponse(msg.ID, []interface{}{})
		return
	}

	syms := symbols.Extract(doc.Content)
	result := make([]documentSymbol, 0, len(syms))
	for _, sym := range syms {
		result = append(result, convertSymbol(sym))
	}
	s.sendResponse(msg.ID, result)
}

func convertSymbol(sym symbols.Symbol) documentSymbol {
	selEnd := sym.Col + len(sym.Name)

	// Full range must contain the selection range.
	// If the block ends on the same line it started, make sure the
	// range end character is past the selection end.
	endLine := sym.EndLine
	endChar := 0
	if endLine < sym.Line || (endLine == sym.Line && endChar <= selEnd) {
		endLine = sym.Line
		endChar = selEnd
	}

	ds := documentSymbol{
		Name:   sym.Name,
		Detail: sym.Detail,
		Kind:   int(sym.Kind),
		Range: lspRange{
			Start: lspPosition{Line: sym.Line, Character: 0},
			End:   lspPosition{Line: endLine, Character: endChar},
		},
		SelectionRange: lspRange{
			Start: lspPosition{Line: sym.Line, Character: sym.Col},
			End:   lspPosition{Line: sym.Line, Character: selEnd},
		},
	}
	if len(sym.Children) > 0 {
		ds.Children = make([]documentSymbol, 0, len(sym.Children))
		for _, child := range sym.Children {
			ds.Children = append(ds.Children, convertSymbol(child))
		}
	}
	return ds
}

// Publish diagnostics to client

func (s *Server) publishDiagnostics(uri string, diags []workspace.LSPDiagnostic) {
	if diags == nil {
		diags = []workspace.LSPDiagnostic{}
	}
	s.sendNotification("textDocument/publishDiagnostics", map[string]interface{}{
		"uri":         uri,
		"diagnostics": diags,
	})
}

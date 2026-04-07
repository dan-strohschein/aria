// Package protocol defines types matching the Aria compiler's JSON diagnostic output
// and provides conversion to LSP protocol types.
package protocol

import (
	"encoding/json"
	"io"
)

type DiagnosticOutput struct {
	Diagnostics []Diagnostic `json:"diagnostics"`
	Summary     Summary      `json:"summary"`
}

type Diagnostic struct {
	Code        string       `json:"code"`
	Severity    string       `json:"severity"`
	Message     string       `json:"message"`
	File        string       `json:"file"`
	Line        int          `json:"line"`
	Column      int          `json:"column"`
	Span        [2]int       `json:"span"`
	SourceLine  string       `json:"source_line"`
	Labels      []Label      `json:"labels"`
	Notes       []string     `json:"notes"`
	Suggestions []Suggestion `json:"suggestions"`
}

type Label struct {
	File    string `json:"file"`
	Line    int    `json:"line"`
	Column  int    `json:"column"`
	Span    [2]int `json:"span"`
	Message string `json:"message"`
	Style   string `json:"style"`
}

type Suggestion struct {
	Message       string `json:"message"`
	Replacement   string `json:"replacement"`
	Span          [2]int `json:"span"`
	Applicability string `json:"applicability"`
}

type Summary struct {
	Errors   int `json:"errors"`
	Warnings int `json:"warnings"`
}

func ParseDiagnostics(r io.Reader) (*DiagnosticOutput, error) {
	var out DiagnosticOutput
	if err := json.NewDecoder(r).Decode(&out); err != nil {
		return nil, err
	}
	return &out, nil
}

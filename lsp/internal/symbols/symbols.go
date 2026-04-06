// Package symbols extracts document symbols from Aria source text using
// lightweight pattern matching. This avoids depending on the compiler's
// parser (which may fail on files with errors) and provides fast outline
// information for the LSP.
package symbols

import (
	"regexp"
	"strings"
)

type SymbolKind int

const (
	KindFunction  SymbolKind = 12
	KindStruct    SymbolKind = 23
	KindEnum      SymbolKind = 10
	KindInterface SymbolKind = 11
	KindConstant  SymbolKind = 14
	KindVariable  SymbolKind = 13
	KindModule    SymbolKind = 2
)

type Symbol struct {
	Name       string
	Kind       SymbolKind
	Detail     string
	Line       int // 0-indexed
	Col        int // 0-indexed
	EndLine    int
	EndCol     int
	Children   []Symbol
}

var (
	fnDecl     = regexp.MustCompile(`^(pub\s+)?fn\s+(\w+)`)
	typeDecl   = regexp.MustCompile(`^(pub\s+)?type\s+(\w+)`)
	structDecl = regexp.MustCompile(`^(pub\s+)?struct\s+(\w+)`)
	traitDecl  = regexp.MustCompile(`^(pub\s+)?trait\s+(\w+)`)
	implDecl   = regexp.MustCompile(`^impl\s+(\w+)(?:\s+for\s+(\w+))?`)
	enumDecl   = regexp.MustCompile(`^(pub\s+)?enum\s+(\w+)`)
	constDecl  = regexp.MustCompile(`^(pub\s+)?const\s+(\w+)`)
	entryDecl  = regexp.MustCompile(`^entry\s*\{`)
	testDecl   = regexp.MustCompile(`^test\s+"([^"]*)"`)
	modDecl    = regexp.MustCompile(`^mod\s+(\w+)`)
)

func Extract(source string) []Symbol {
	lines := strings.Split(source, "\n")
	var symbols []Symbol

	for i, line := range lines {
		trimmed := strings.TrimSpace(line)
		if trimmed == "" || strings.HasPrefix(trimmed, "//") {
			continue
		}

		if m := fnDecl.FindStringSubmatch(trimmed); m != nil {
			col := strings.Index(line, "fn")
			symbols = append(symbols, Symbol{
				Name:    m[2],
				Kind:    KindFunction,
				Detail:  extractSignature(trimmed),
				Line:    i,
				Col:     col,
				EndLine: findBlockEnd(lines, i),
			})
		} else if m := structDecl.FindStringSubmatch(trimmed); m != nil {
			col := strings.Index(line, "struct")
			sym := Symbol{
				Name:    m[2],
				Kind:    KindStruct,
				Line:    i,
				Col:     col,
				EndLine: findBlockEnd(lines, i),
			}
			sym.Children = extractFields(lines, i)
			symbols = append(symbols, sym)
		} else if m := typeDecl.FindStringSubmatch(trimmed); m != nil {
			col := strings.Index(line, "type")
			kind := KindEnum
			// Check if it's a sum type (has |) or newtype/alias
			rest := trimmed[strings.Index(trimmed, "=")+1:]
			if !strings.Contains(rest, "|") {
				kind = KindVariable // newtype/alias
			}
			symbols = append(symbols, Symbol{
				Name:    m[2],
				Kind:    kind,
				Line:    i,
				Col:     col,
				EndLine: findBlockEnd(lines, i),
			})
		} else if m := traitDecl.FindStringSubmatch(trimmed); m != nil {
			col := strings.Index(line, "trait")
			symbols = append(symbols, Symbol{
				Name:    m[2],
				Kind:    KindInterface,
				Line:    i,
				Col:     col,
				EndLine: findBlockEnd(lines, i),
			})
		} else if m := implDecl.FindStringSubmatch(trimmed); m != nil {
			col := strings.Index(line, "impl")
			name := m[1]
			if m[2] != "" {
				name = m[1] + " for " + m[2]
			}
			symbols = append(symbols, Symbol{
				Name:    name,
				Kind:    KindStruct,
				Detail:  "impl",
				Line:    i,
				Col:     col,
				EndLine: findBlockEnd(lines, i),
			})
		} else if m := enumDecl.FindStringSubmatch(trimmed); m != nil {
			col := strings.Index(line, "enum")
			symbols = append(symbols, Symbol{
				Name:    m[2],
				Kind:    KindEnum,
				Line:    i,
				Col:     col,
				EndLine: findBlockEnd(lines, i),
			})
		} else if m := constDecl.FindStringSubmatch(trimmed); m != nil {
			col := strings.Index(line, "const")
			symbols = append(symbols, Symbol{
				Name:    m[2],
				Kind:    KindConstant,
				Line:    i,
				Col:     col,
				EndLine: i,
			})
		} else if entryDecl.MatchString(trimmed) {
			col := strings.Index(line, "entry")
			symbols = append(symbols, Symbol{
				Name:    "entry",
				Kind:    KindFunction,
				Detail:  "entry point",
				Line:    i,
				Col:     col,
				EndLine: findBlockEnd(lines, i),
			})
		} else if m := testDecl.FindStringSubmatch(trimmed); m != nil {
			col := strings.Index(line, "test")
			symbols = append(symbols, Symbol{
				Name:    "test " + m[1],
				Kind:    KindFunction,
				Detail:  "test",
				Line:    i,
				Col:     col,
				EndLine: findBlockEnd(lines, i),
			})
		} else if m := modDecl.FindStringSubmatch(trimmed); m != nil {
			symbols = append(symbols, Symbol{
				Name:    m[1],
				Kind:    KindModule,
				Line:    i,
				Col:     0,
				EndLine: i,
			})
		}
	}

	return symbols
}

func extractSignature(line string) string {
	// Extract up to the opening brace or = for single-line fns
	if idx := strings.Index(line, "{"); idx != -1 {
		return strings.TrimSpace(line[:idx])
	}
	if idx := strings.Index(line, "="); idx != -1 {
		return strings.TrimSpace(line[:idx])
	}
	return line
}

func extractFields(lines []string, startLine int) []Symbol {
	var fields []Symbol
	braceDepth := 0
	fieldRe := regexp.MustCompile(`^\s+(\w+)\s*:`)

	for i := startLine; i < len(lines); i++ {
		line := lines[i]
		for _, ch := range line {
			if ch == '{' {
				braceDepth++
			} else if ch == '}' {
				braceDepth--
				if braceDepth == 0 {
					return fields
				}
			}
		}
		if braceDepth == 1 {
			if m := fieldRe.FindStringSubmatch(line); m != nil {
				fields = append(fields, Symbol{
					Name:    m[1],
					Kind:    KindVariable,
					Line:    i,
					Col:     strings.Index(line, m[1]),
					EndLine: i,
				})
			}
		}
	}
	return fields
}

func findBlockEnd(lines []string, startLine int) int {
	braceDepth := 0
	for i := startLine; i < len(lines); i++ {
		for _, ch := range lines[i] {
			if ch == '{' {
				braceDepth++
			} else if ch == '}' {
				braceDepth--
				if braceDepth == 0 {
					return i
				}
			}
		}
	}
	return startLine
}

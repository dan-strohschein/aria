package main

import (
	"flag"
	"fmt"
	"log"
	"os"
	"os/exec"

	"github.com/aria-lang/aria-lsp/internal/server"
)

func main() {
	compilerPath := flag.String("compiler", "", "Path to the aria compiler binary")
	version := flag.Bool("version", false, "Print version and exit")
	flag.Parse()

	if *version {
		fmt.Println("aria-lsp 0.1.0")
		os.Exit(0)
	}

	// Resolve compiler path
	cp := *compilerPath
	if cp == "" {
		cp = os.Getenv("ARIA_COMPILER")
	}
	if cp == "" {
		// Try to find aria on PATH
		found, err := exec.LookPath("aria")
		if err != nil {
			log.Fatal("aria-lsp: cannot find aria compiler. Set --compiler flag or ARIA_COMPILER env var.")
		}
		cp = found
	}

	s := server.New(cp)
	if err := s.Run(); err != nil {
		log.Fatalf("aria-lsp: %v", err)
	}
}

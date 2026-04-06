BINARY    := aria-lsp
GO        := go
GOFLAGS   :=
COMPILER  := ../../aria-compiler-go
VSCODE    := ../vscode

.PHONY: build clean test vet compiler vscode install all

build:
	$(GO) build $(GOFLAGS) -o $(BINARY) ./cmd/aria-lsp

clean:
	rm -f $(BINARY)

test:
	$(GO) test ./...

vet:
	$(GO) vet ./...

compiler:
	cd $(COMPILER) && $(GO) build -o aria ./cmd/aria

vscode:
	cd $(VSCODE) && npm install && npx tsc -p ./

install: build
	cp $(BINARY) $(GOPATH)/bin/ 2>/dev/null || cp $(BINARY) ~/go/bin/

all: compiler build vscode

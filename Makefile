BINARY       := aria-lsp
GO           := go
GOFLAGS      :=
LSP_DIR      := lsp
VSCODE_DIR   := vscode
COMPILER_DIR := ../aria-compiler-go
SERVER_DIR   := $(VSCODE_DIR)/server

.PHONY: build clean test vet lsp compiler vscode vsix install all

# Build everything and produce the .vsix
all: compiler lsp vscode vsix

# Build the LSP server binary
lsp:
	cd $(LSP_DIR) && $(GO) build $(GOFLAGS) -o $(BINARY) ./cmd/aria-lsp

# Build the bootstrap compiler
compiler:
	cd $(COMPILER_DIR) && $(GO) build -o aria ./cmd/aria

# Compile the VSCode extension TypeScript
vscode:
	cd $(VSCODE_DIR) && npm install && npx tsc -p ./

# Bundle the LSP binary into the extension and produce a .vsix
vsix: lsp vscode
	mkdir -p $(SERVER_DIR)
	cp $(LSP_DIR)/$(BINARY) $(SERVER_DIR)/$(BINARY)
	chmod +x $(SERVER_DIR)/$(BINARY)
	cd $(VSCODE_DIR) && npx @vscode/vsce package --allow-missing-repository -o ../aria-lang.vsix

# Install the .vsix into VSCode
install: vsix
	code --install-extension aria-lang.vsix

# Run LSP server tests
test:
	cd $(LSP_DIR) && $(GO) test ./...

# Run go vet on the LSP server
vet:
	cd $(LSP_DIR) && $(GO) vet ./...

# Clean build artifacts
clean:
	rm -f $(LSP_DIR)/$(BINARY)
	rm -rf $(SERVER_DIR)
	rm -rf $(VSCODE_DIR)/out
	rm -f aria-lang.vsix

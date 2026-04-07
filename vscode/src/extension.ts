import * as path from "path";
import * as fs from "fs";
import { workspace, ExtensionContext, window } from "vscode";
import {
  LanguageClient,
  LanguageClientOptions,
  ServerOptions,
  TransportKind,
} from "vscode-languageclient/node";

let client: LanguageClient | undefined;

function findBundledServer(context: ExtensionContext): string | undefined {
  const bundled = path.join(context.extensionPath, "server", "aria-lsp");
  if (fs.existsSync(bundled)) {
    return bundled;
  }
  return undefined;
}

export function activate(context: ExtensionContext): void {
  const config = workspace.getConfiguration("aria");
  const configuredPath = config.inspect<string>("lsp.path");
  // Only use the configured path if the user explicitly set it
  const userSetPath =
    configuredPath?.workspaceValue ?? configuredPath?.globalValue;
  const lspPath = userSetPath || findBundledServer(context) || "aria-lsp";
  const compilerPath = config.get<string>("compiler.path", "aria");

  const serverOptions: ServerOptions = {
    command: lspPath,
    args: ["--compiler", compilerPath, "--stdio"],
  };

  const clientOptions: LanguageClientOptions = {
    documentSelector: [{ scheme: "file", language: "aria" }],
    synchronize: {
      fileEvents: workspace.createFileSystemWatcher("**/*.aria"),
    },
  };

  client = new LanguageClient(
    "aria",
    "Aria Language Server",
    serverOptions,
    clientOptions
  );

  client.start().catch((err) => {
    window.showErrorMessage(
      `Failed to start Aria language server: ${err.message}. ` +
        `Make sure aria-lsp is installed and the path is correct in settings.`
    );
  });
}

export function deactivate(): Thenable<void> | undefined {
  if (!client) {
    return undefined;
  }
  return client.stop();
}

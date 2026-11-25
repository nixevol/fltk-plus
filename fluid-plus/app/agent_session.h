// One-shot project commands and the MCP server. Both live in this executable.

#ifndef FLUID_APP_AGENT_SESSION_H
#define FLUID_APP_AGENT_SESSION_H

namespace fluid {

// fluid-plus --agent <command> ...
// Reads one .fl, prints one JSON object to stdout, and exits.
int agent_cli_main(int argc, char **argv);

// fluid-plus --mcp
// Speaks MCP on stdin/stdout. Each tool call reloads the .fl from disk.
int agent_mcp_main();

// Help menu. Shows a client-neutral server entry for the running executable.
void show_mcp_setup_help();

}

#endif

# Monolith MCP (UE 5.7)

The project contains Monolith v0.24.0 source under `Plugins/Monolith`. Its MIT
license is included there. Unreal and proxy binaries are ignored by Git.

On Windows, after a fresh checkout, close Unreal Editor and run:

```powershell
./Tools/SetupMonolith.ps1
```

The script downloads the official UE 5.7 release and verifies its SHA-256 before
restoring its binaries. Rebuild the C++ project if Unreal requests a rebuild.
Start the editor and restart the MCP client so it reads `.mcp.json`. The editor
log should report that the Monolith MCP server is listening on port 9316.
The initial project index may take a little time.

Monolith's HTTP listener is reachable from network interfaces, not just
localhost. Restrict inbound TCP port 9316 with the firewall on untrusted
networks. See `Plugins/Monolith/SECURITY.md` for details.

Upstream: https://github.com/tumourlove/monolith/releases/tag/v0.24.0

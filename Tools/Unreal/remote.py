#!/usr/bin/env python3
# remote.py
# 역할: 프로젝트와 엔진 버전이 일치하는 에디터에만 Python 명령을 전달한다.

"""Connect to this checkout's project-selected editor using Epic's bundled Python client."""
import argparse
import json
import math
import os
from pathlib import Path
import subprocess
import sys
import time

PROJECT = Path(__file__).resolve().parents[2] / "FestaFiesta.uproject"
MODULE = Path("Engine/Plugins/Experimental/PythonScriptPlugin/Content/Python")
MARKER = "FF_UNREAL_STATUS="


def engine_association():
    """프로젝트 파일이 선택한 엔진 버전을 연결·탐색·검증 모두에 사용한다."""
    return str(json.loads(PROJECT.read_text(encoding="utf-8-sig"))["EngineAssociation"])


def normalized(path):
    return os.path.normcase(os.path.realpath(path))


def engine_root(explicit):
    association = engine_association()
    try:
        expected_version = tuple(int(part) for part in association.split("."))
    except ValueError as error:
        raise RuntimeError("Remote client needs a numeric EngineAssociation such as 5.7") from error
    if len(expected_version) != 2:
        raise RuntimeError("EngineAssociation must contain major.minor")
    selected = explicit or os.environ.get("FESTAFIESTA_UE_ROOT")
    candidates = [Path(selected)] if selected else []
    manifest = Path(os.environ.get("PROGRAMDATA", "C:/ProgramData")) / "Epic/UnrealEngineLauncher/LauncherInstalled.dat"
    if not selected and manifest.is_file():
        installs = json.loads(manifest.read_text(encoding="utf-8-sig"))["InstallationList"]
        candidates += [Path(item["InstallLocation"]) for item in installs
                       if item.get("AppName") == "UE_" + association]
    if not selected:
        candidates.append(Path(os.environ.get("PROGRAMFILES", "C:/Program Files")) / f"Epic Games/UE_{association}")
    for candidate in candidates:
        version_file = candidate / "Engine/Build/Build.version"
        if not version_file.is_file() or not (candidate / MODULE / "remote_execution.py").is_file():
            continue
        version = json.loads(version_file.read_text(encoding="utf-8-sig"))
        if (version.get("MajorVersion"), version.get("MinorVersion")) == expected_version:
            return candidate.resolve()
    raise RuntimeError(f"UE {association} not found; supply --engine-root or FESTAFIESTA_UE_ROOT")


def guarded(code):
    # Check again inside each command, before any caller-supplied Python can run.
    return (
        "import os as _ff_os, unreal as _ff_ue\n"
        "_ff_project = _ff_ue.Paths.convert_relative_path_to_full(_ff_ue.Paths.get_project_file_path())\n"
        f"if _ff_os.path.normcase(_ff_os.path.realpath(_ff_project)) != {normalized(PROJECT)!r}:\n"
        "    raise RuntimeError('Project mismatch: refusing to execute')\n" + code
    )


def connect(args):
    engine = engine_root(args.engine_root)
    sys.path.insert(0, str(engine / MODULE))
    import remote_execution

    config = remote_execution.RemoteExecutionConfig()
    config.multicast_bind_address = "127.0.0.1"
    config.multicast_ttl = 0
    config.command_endpoint = ("127.0.0.1", 6776)
    session = remote_execution.RemoteExecution(config)
    deadline = time.monotonic() + args.timeout
    first_match = None
    try:
        session.start()
        while time.monotonic() < deadline:
            matches = [node for node in session.remote_nodes
                       if node.get("project_root") and normalized(node["project_root"]) == normalized(PROJECT.parent)
                       and node.get("project_name", "").casefold() == PROJECT.stem.casefold()
                       and node.get("engine_version", "").startswith(engine_association() + ".")]
            if len(matches) > 1:
                raise RuntimeError("Multiple editors have this project open; close duplicate instances")
            if matches:
                first_match = first_match or time.monotonic()
                if time.monotonic() - first_match >= 1.2:
                    break
            else:
                first_match = None
            time.sleep(0.1)
        else:
            raise TimeoutError("This checkout's project-selected editor was not discovered on loopback")
        node = matches[0]
        session.open_command_connection(node["node_id"])
        # Epic's client exposes no timeout setter and otherwise blocks indefinitely.
        session._command_connection._command_channel_socket.settimeout(max(0.1, deadline - time.monotonic()))
        status_code = (
            "import json as _ff_json\n"
            "_ff_world = _ff_ue.get_editor_subsystem(_ff_ue.UnrealEditorSubsystem).get_editor_world()\n"
            f"print({MARKER!r} + _ff_json.dumps({{"
            "'engine_version': _ff_ue.SystemLibrary.get_engine_version(), "
            "'project_file': _ff_project, "
            "'world_path': _ff_world.get_path_name() if _ff_world else None, "
            "'actor_count': len(_ff_ue.get_editor_subsystem(_ff_ue.EditorActorSubsystem).get_all_level_actors())"
            "}, ensure_ascii=False))\n"
        )
        response = session.run_command(guarded(status_code), exec_mode=remote_execution.MODE_EXEC_FILE)
        if not response.get("success"):
            return {"success": False, "error": "Editor identity/status check failed", "response": response}
        statuses = [item["output"].split(MARKER, 1)[1].strip() for item in response.get("output", [])
                    if item.get("output", "").startswith(MARKER)]
        if len(statuses) != 1:
            raise RuntimeError("Editor did not return exactly one verified status record")
        status = json.loads(statuses[0])
        if normalized(status["project_file"]) != normalized(PROJECT):
            raise RuntimeError("Editor project verification failed")
        result = {"success": True, "node_id": node["node_id"], "status": status}
        if args.code is not None or args.script:
            filename = str(Path(args.script).resolve()) if args.script else "<remote-code>"
            code = Path(filename).read_text(encoding="utf-8-sig") if args.script else args.code
            command = f"exec(compile({code!r}, {filename!r}, 'exec'), {{'__name__': '__main__', '__file__': {filename!r}}})\n"
            response = session.run_command(guarded(command), exec_mode=remote_execution.MODE_EXEC_FILE)
            result.update(success=bool(response.get("success")), response=response)
        return result
    finally:
        session.stop()


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--engine-root", help="Project engine installation directory (auto-detected by default)")
    parser.add_argument("--timeout", type=float, default=30, help="Total client timeout in seconds (default: 30)")
    operation = parser.add_mutually_exclusive_group()
    operation.add_argument("--code", help="Python code to run after verifying this project")
    operation.add_argument("--script", help="UTF-8 Python file to run after verifying this project")
    parser.add_argument("--_worker", action="store_true", help=argparse.SUPPRESS)
    args = parser.parse_args()
    if not math.isfinite(args.timeout) or args.timeout <= 0:
        parser.error("--timeout must be a positive finite number")
    try:
        if args._worker:
            result = connect(args)
        else:
            # Bound discovery, Epic's fixed 30-second TCP accept, and command execution.
            process = subprocess.run([sys.executable, str(Path(__file__).resolve()), *sys.argv[1:], "--_worker"],
                                     capture_output=True, text=True, encoding="utf-8", timeout=args.timeout,
                                     env={**os.environ, "PYTHONIOENCODING": "utf-8"})
            if not process.stdout.strip():
                raise RuntimeError(process.stderr.strip() or "Remote client exited without a result")
            result = json.loads(process.stdout)
    except subprocess.TimeoutExpired:
        result = {"success": False, "error": "Client timeout; an already-sent command may still run in the editor", "timeout": True}
    except Exception as error:
        result = {"success": False, "error": str(error)}
    print(json.dumps(result, ensure_ascii=False, indent=2))
    return 0 if result.get("success") else 1


if __name__ == "__main__":
    raise SystemExit(main())

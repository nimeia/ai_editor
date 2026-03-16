#!/usr/bin/env python3
from __future__ import annotations
import json
import subprocess
from dataclasses import dataclass
from pathlib import Path
from typing import Any, Optional


@dataclass
class BridgeCliError(RuntimeError):
    code: str
    message: str
    payload: Optional[dict[str, Any]] = None

    def __str__(self) -> str:
        return f"{self.code}: {self.message}"


class BridgeClient:
    def __init__(self, cli_path: str, workspace: str, *, session_id: str = "sdk-session", client_id: str = "sdk-python") -> None:
        self.cli_path = cli_path
        self.workspace = workspace
        self.session_id = session_id
        self.client_id = client_id

    def _run(self, *args: str) -> dict[str, Any]:
        cmd = [self.cli_path, *args, "--workspace", self.workspace, "--session-id", self.session_id, "--client-id", self.client_id, "--json"]
        proc = subprocess.run(cmd, text=True, capture_output=True)
        try:
            payload = json.loads(proc.stdout)
        except json.JSONDecodeError as exc:
            raise RuntimeError(proc.stderr or proc.stdout or str(exc)) from exc
        if proc.returncode != 0 or not payload.get("ok", False):
            err = payload.get("error", {})
            raise BridgeCliError(err.get("code", "CLI_ERROR"), err.get("message", "request failed"), payload)
        return payload["result"]

    @staticmethod
    def _write_temp(text: str, suffix: str = ".txt") -> str:
        import tempfile
        fd, path = tempfile.mkstemp(prefix="bridge_sdk_", suffix=suffix)
        Path(path).write_text(text, encoding="utf-8")
        return path

    def session_begin(self) -> dict[str, Any]:
        return self._run("session-begin")

    def session_preview(self) -> dict[str, Any]:
        return self._run("session-preview")

    def session_commit(self) -> dict[str, Any]:
        return self._run("session-commit")

    def session_inspect(self) -> dict[str, Any]:
        return self._run("session-inspect")

    def session_drop_change(self, change_id: str) -> dict[str, Any]:
        return self._run("session-drop-change", "--change-id", change_id)

    def session_drop_path(self, path: str) -> dict[str, Any]:
        return self._run("session-drop-path", "--path", path)

    def markdown_upsert_section(self, path: str, heading: str, body: str, *, heading_level: int = 2) -> dict[str, Any]:
        temp = self._write_temp(body, ".md")
        return self._run("markdown-upsert-section", "--path", path, "--heading", heading, "--heading-level", str(heading_level), "--content-file", temp)

    def json_upsert_key(self, path: str, key_path: str, value_json: str) -> dict[str, Any]:
        temp = self._write_temp(value_json, ".json")
        return self._run("json-upsert-key", "--path", path, "--key-path", key_path, "--content-file", temp)

    def yaml_append_item(self, path: str, key_path: str, value_text: str) -> dict[str, Any]:
        temp = self._write_temp(value_text, ".yaml")
        return self._run("yaml-append-item", "--path", path, "--key-path", key_path, "--content-file", temp)

    def html_set_attribute(self, path: str, selector: str, attribute_name: str, attribute_value: str) -> dict[str, Any]:
        return self._run("html-set-attribute", "--path", path, "--query", selector, "--attr-name", attribute_name, "--attr-value", attribute_value)

import { execFileSync } from 'node:child_process';
import { writeFileSync } from 'node:fs';
import { tmpdir } from 'node:os';
import { join } from 'node:path';

export class BridgeCliError extends Error {
  constructor(code, message, payload = null) {
    super(`${code}: ${message}`);
    this.code = code;
    this.payload = payload;
  }
}

export class BridgeClient {
  constructor(cliPath, workspace, { sessionId = 'sdk-session', clientId = 'sdk-ts' } = {}) {
    this.cliPath = cliPath;
    this.workspace = workspace;
    this.sessionId = sessionId;
    this.clientId = clientId;
  }

  _run(...args) {
    const output = execFileSync(this.cliPath, [...args, '--workspace', this.workspace, '--session-id', this.sessionId, '--client-id', this.clientId, '--json'], { encoding: 'utf8' });
    const payload = JSON.parse(output);
    if (!payload.ok) {
      const err = payload.error || {};
      throw new BridgeCliError(err.code || 'CLI_ERROR', err.message || 'request failed', payload);
    }
    return payload.result;
  }

  _writeTemp(text, suffix = '.txt') {
    const path = join(tmpdir(), `bridge_sdk_${Date.now()}_${Math.random().toString(16).slice(2)}${suffix}`);
    writeFileSync(path, text, 'utf8');
    return path;
  }

  sessionBegin() { return this._run('session-begin'); }
  sessionPreview() { return this._run('session-preview'); }
  sessionCommit() { return this._run('session-commit'); }
  sessionInspect() { return this._run('session-inspect'); }
  sessionDropChange(changeId) { return this._run('session-drop-change', '--change-id', changeId); }
  sessionDropPath(path) { return this._run('session-drop-path', '--path', path); }

  markdownUpsertSection(path, heading, body, headingLevel = 2) {
    return this._run('markdown-upsert-section', '--path', path, '--heading', heading, '--heading-level', String(headingLevel), '--content-file', this._writeTemp(body, '.md'));
  }

  jsonUpsertKey(path, keyPath, valueJson) {
    return this._run('json-upsert-key', '--path', path, '--key-path', keyPath, '--content-file', this._writeTemp(valueJson, '.json'));
  }

  yamlAppendItem(path, keyPath, valueText) {
    return this._run('yaml-append-item', '--path', path, '--key-path', keyPath, '--content-file', this._writeTemp(valueText, '.yaml'));
  }

  htmlSetAttribute(path, selector, attributeName, attributeValue) {
    return this._run('html-set-attribute', '--path', path, '--query', selector, '--attr-name', attributeName, '--attr-value', attributeValue);
  }
}

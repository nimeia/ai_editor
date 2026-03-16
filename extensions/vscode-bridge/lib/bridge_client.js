const { execFile } = require('node:child_process');
const { promisify } = require('node:util');

const execFileAsync = promisify(execFile);

class BridgeCliError extends Error {
  constructor(code, message, payload = null) {
    super(`${code}: ${message}`);
    this.name = 'BridgeCliError';
    this.code = code;
    this.payload = payload;
  }
}

class BridgeCliBridge {
  constructor(cliPath, workspace, options = {}) {
    this.cliPath = cliPath;
    this.workspace = workspace;
    this.sessionId = options.sessionId || 'vscode-session';
    this.clientId = options.clientId || 'vscode-extension';
  }

  async run(command, ...args) {
    const finalArgs = [
      ...args,
      '--workspace', this.workspace,
      '--session-id', this.sessionId,
      '--client-id', this.clientId,
      '--json',
    ];

    let stdout = '';
    let stderr = '';
    try {
      const result = await execFileAsync(this.cliPath, [command, ...finalArgs], {
        encoding: 'utf8',
        maxBuffer: 8 * 1024 * 1024,
      });
      stdout = result.stdout || '';
      stderr = result.stderr || '';
    } catch (error) {
      stdout = error.stdout || '';
      stderr = error.stderr || error.message || '';
    }

    let payload;
    try {
      payload = JSON.parse(stdout);
    } catch (error) {
      throw new BridgeCliError('CLI_IO_ERROR', stderr || stdout || String(error));
    }

    if (!payload.ok) {
      const err = payload.error || {};
      throw new BridgeCliError(err.code || 'CLI_ERROR', err.message || 'request failed', payload);
    }

    return payload.result;
  }

  searchText(query, options = {}) {
    const args = ['--query', query];
    if (options.path) args.push('--path', options.path);
    if (options.maxResults) args.push('--max-results', String(options.maxResults));
    return this.run('search-text', ...args);
  }

  sessionBegin() { return this.run('session-begin'); }
  sessionInspect() { return this.run('session-inspect'); }
  sessionPreview() { return this.run('session-preview'); }
  sessionCommit() { return this.run('session-commit'); }
  sessionAbort() { return this.run('session-abort'); }
  sessionRecover() { return this.run('session-recover'); }
  recoveryCheck() { return this.run('recovery-check'); }
  recoveryRebase() { return this.run('recovery-rebase'); }

  editReplaceRange(path, startLine, endLine, contentFile) {
    return this.run('edit-replace-range', '--path', path, '--start', String(startLine), '--end', String(endLine), '--content-file', contentFile);
  }

  editReplaceBlock(path, query, contentFile, anchor) {
    const args = ['--path', path, '--query', query, '--content-file', contentFile];
    if (anchor) args.push('--anchor-before', anchor);
    return this.run('edit-replace-block', ...args);
  }

  sessionDropChange(changeId) {
    return this.run('session-drop-change', '--change-id', changeId);
  }

  sessionDropPath(path) {
    return this.run('session-drop-path', '--path', path);
  }
}

function escapeHtml(value) {
  return String(value)
    .replace(/&/g, '&amp;')
    .replace(/</g, '&lt;')
    .replace(/>/g, '&gt;')
    .replace(/"/g, '&quot;');
}

function renderPreviewHtml(preview) {
  const files = (preview.files || []).map((file) => `
    <section>
      <h2>${escapeHtml(file.path)} <small>risk=${escapeHtml(file.risk_level || 'low')}</small></h2>
      <p>${escapeHtml(file.summary || '')}</p>
      <pre>${escapeHtml(file.diff || '')}</pre>
    </section>`).join('\n');

  const highlights = (preview.highlights || []).map((item) => `<li>${escapeHtml(item)}</li>`).join('');

  return `<!doctype html><html><body><h1>AI Bridge Session Preview</h1><p>${escapeHtml(preview.summary || '')}</p><p>Overall risk: <strong>${escapeHtml(preview.risk_level || 'low')}</strong></p><ul>${highlights}</ul>${files}</body></html>`;
}

module.exports = { BridgeCliBridge, BridgeCliError, renderPreviewHtml };

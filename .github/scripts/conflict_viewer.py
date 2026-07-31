#!/usr/bin/env python3
"""
Backport Conflict Viewer Generator
Reads conflict data from BP_* environment variables, calls GitHub Models API
for an AI resolution suggestion, generates a self-contained interactive HTML
page, and pushes it to the conflict-reports branch via the GitHub Contents API.

Outputs written:
  /tmp/bp_ai.txt   — AI suggestion text (always written)
  /tmp/bp_url.txt  — Key=value lines: PAGES=..., RAW=..., GITHUB=...
"""

import os
import json
import base64
import html as H
import urllib.request as U
import urllib.error
import sys

# ── Read environment variables ─────────────────────────────────────────────
E = os.environ.get
cm         = E('BP_CM',    '(no commit message)')
short      = E('BP_SHORT', '?')
sha        = E('BP_SHA',   '')
tf         = E('BP_TF',    '(unknown file)')
rl         = E('BP_RL',    '0')
branch     = E('BP_BRANCH','main')
sd         = E('BP_SD',    '')   # source diff lines
tc         = E('BP_TC',    '')   # target context lines
repo       = E('BP_REPO',  '')
run_id     = E('BP_RUNID', '')
tok         = E('BP_TOK',        '')
models_tok  = E('MODELS_TOKEN',  '')
ollama_url  = E('OLLAMA_URL',    'http://localhost:11434').rstrip('/')
auto_apply  = E('BP_AUTO_APPLY', 'false').lower() == 'true'

# ── Build shared prompt ────────────────────────────────────────────────────
_prompt = (
    "Resolve this git backport conflict. Output ONLY the corrected replacement "
    "code lines for the target file — no explanation, no markdown, just the code.\n\n"
    f"Commit being backported: {cm}\n"
    f"File: {tf}   Conflict at line: {rl}\n\n"
    f"What the commit wants to change (diff — lines starting with - are removed, + are added):\n"
    f"{sd}\n\n"
    f"What the target file currently has around the conflict line:\n"
    f"{tc}\n\n"
    "Corrected replacement code:"
)

# ── 1. Call Anthropic Claude API for AI suggestion ─────────────────────────
ai_suggestion = '(No ai_token or ollama_url provided — fill in the workflow dispatch form to enable AI suggestions)'
if models_tok:
    payload = json.dumps({
        'model': 'claude-sonnet-4-5',
        'max_tokens': 500,
        'messages': [{'role': 'user', 'content': _prompt}]
    }).encode()
    req = U.Request(
        'https://api.anthropic.com/v1/messages',
        data=payload,
        headers={
            'x-api-key': models_tok,
            'anthropic-version': '2023-06-01',
            'Content-Type': 'application/json'
        }
    )
    try:
        with U.urlopen(req, timeout=25) as r:
            data = json.load(r)
            ai_suggestion = data['content'][0]['text']
        print('AI: suggestion received from Anthropic Claude API')
    except Exception as e:
        ai_suggestion = f'(Anthropic call failed: {e})'
        print(f'AI WARNING (Anthropic): {e}', file=sys.stderr)

# ── 1b. Fallback: Ollama local model (used if Anthropic key not provided) ──
elif ollama_url:
    _ollama_model = 'phi3:3.8b'

    def _ollama_chat(url, model, prompt):
        """Try /api/chat (modern Ollama), fall back to /api/generate (older)."""
        # Try modern /api/chat endpoint first
        payload = json.dumps({
            'model': model,
            'messages': [{'role': 'user', 'content': prompt}],
            'stream': False
        }).encode()
        try:
            req = U.Request(f'{url}/api/chat', data=payload,
                            headers={'Content-Type': 'application/json'})
            with U.urlopen(req, timeout=120) as r:
                data = json.load(r)
                return data['message']['content'], 'chat'
        except urllib.error.HTTPError as e:
            if e.code != 404:
                raise
        # Fall back to /api/generate (Ollama < 0.1.14)
        payload = json.dumps({
            'model': model,
            'prompt': prompt,
            'stream': False
        }).encode()
        req = U.Request(f'{url}/api/generate', data=payload,
                        headers={'Content-Type': 'application/json'})
        with U.urlopen(req, timeout=120) as r:
            data = json.load(r)
            return data['response'], 'generate'

    try:
        ai_suggestion, _ep = _ollama_chat(ollama_url, _ollama_model, _prompt)
        print(f'AI: suggestion received from Ollama/{_ep} at {ollama_url}')
    except Exception as e:
        ai_suggestion = f'(Ollama call failed: {e})'
        print(f'AI WARNING (Ollama): {e}', file=sys.stderr)

# ── Strip markdown code fences from any AI response ───────────────────────
import re as _re
ai_suggestion = _re.sub(r'```[a-zA-Z]*\n?', '', ai_suggestion).strip('`').strip()

with open('/tmp/bp_ai.txt', 'w') as f:
    f.write(ai_suggestion)

# ── Auto-apply AI fix to the target file ──────────────────────────────────
auto_applied = False
if auto_apply and ai_suggestion and not ai_suggestion.startswith('('):
    try:
        target_line = int(rl) if rl.isdigit() else 0
        if target_line > 0 and os.path.isfile(tf):
            # Extract the lines to replace: '-' lines from source diff (current file content)
            old_lines = [l[1:] for l in sd.splitlines()
                         if l.startswith('-') and not l.startswith('---')]
            if old_lines:
                with open(tf, 'r', errors='replace') as fh:
                    file_lines = fh.readlines()
                # Find the block to replace starting near the conflict line
                search_start = max(0, target_line - 10)
                search_end   = min(len(file_lines), target_line + 10)
                matched_idx  = None
                for idx in range(search_start, search_end):
                    if file_lines[idx].rstrip('\n').strip() == old_lines[0].strip():
                        matched_idx = idx
                        break
                if matched_idx is not None:
                    fix_lines = [l if l.endswith('\n') else l + '\n'
                                 for l in ai_suggestion.splitlines()]
                    file_lines[matched_idx : matched_idx + len(old_lines)] = fix_lines
                    with open(tf, 'w', errors='replace') as fh:
                        fh.writelines(file_lines)
                    auto_applied = True
                    print(f'AUTO-APPLY: fix written to {tf} at line {matched_idx + 1}')
                else:
                    print('AUTO-APPLY: could not locate conflict lines in file — skipped', file=sys.stderr)
            else:
                print('AUTO-APPLY: no "-" lines in source diff — skipped', file=sys.stderr)
        else:
            print('AUTO-APPLY: invalid line number or file not found — skipped', file=sys.stderr)
    except Exception as e:
        print(f'AUTO-APPLY ERROR: {e}', file=sys.stderr)

with open('/tmp/bp_auto_applied.txt', 'w') as f:
    f.write('true' if auto_applied else 'false')

# ── 2. Generate interactive HTML conflict viewer ───────────────────────────
def diff_html(lines):
    """Colour-code diff lines: green for +, red for -, plain for context."""
    out = []
    for line in lines.splitlines():
        esc = H.escape(line)
        if line.startswith('+'):
            out.append(f'<span style="color:#3fb950">{esc}</span>')
        elif line.startswith('-'):
            out.append(f'<span style="color:#f85149">{esc}</span>')
        else:
            out.append(f'<span style="color:#e6edf3">{esc}</span>')
    return '\n'.join(out)

rl_int        = int(rl) if rl.isdigit() else 0
replace_start = max(1, rl_int - 3)
replace_end   = rl_int + 6

# JavaScript uses plain string concatenation (no template literals with $)
# to avoid GitHub Actions ${{ }} expression processing in YAML.
page = f"""<!DOCTYPE html>
<html lang="en">
<head>
<meta charset="UTF-8">
<meta name="viewport" content="width=device-width,initial-scale=1">
<title>Conflict: {H.escape(tf)}</title>
<style>
*{{box-sizing:border-box;margin:0;padding:0}}
body{{background:#0d1117;color:#e6edf3;font-family:-apple-system,BlinkMacSystemFont,'Segoe UI',sans-serif;padding:24px;line-height:1.6}}
a{{color:#58a6ff;text-decoration:none}}
h1{{font-size:22px;font-weight:700;margin-bottom:4px}}
.sub{{color:#8b949e;font-size:13px;margin-bottom:28px}}
.card{{background:#161b22;border:1px solid #30363d;border-radius:8px;margin-bottom:16px;overflow:hidden}}
.ch{{padding:12px 16px;border-bottom:1px solid #30363d;font-weight:600;font-size:14px}}
.cb{{padding:16px}}
.grid{{display:grid;grid-template-columns:1fr 1fr;gap:0}}
.col{{padding:12px 16px}}
.col:first-child{{border-right:1px solid #30363d}}
.lbl{{font-size:11px;font-weight:700;text-transform:uppercase;letter-spacing:.06em;margin-bottom:8px}}
pre{{font-family:'SF Mono',Consolas,monospace;font-size:12px;line-height:1.65;white-space:pre-wrap;word-break:break-word}}
.badge{{display:inline-flex;align-items:center;padding:2px 8px;border-radius:20px;font-size:12px;font-weight:600}}
.red{{background:#3d1e1e;color:#f85149}}
.blue{{background:#1c2a3a;color:#58a6ff}}
.meta-row{{display:flex;gap:24px;flex-wrap:wrap;padding:16px}}
.meta-item{{display:flex;flex-direction:column;gap:2px}}
.meta-lbl{{font-size:11px;color:#8b949e;text-transform:uppercase;letter-spacing:.05em}}
.meta-val{{font-size:13px;font-family:monospace}}
.ai-pre{{color:#bc8cff}}
input[type=password]{{width:100%;background:#0d1117;color:#e6edf3;border:1px solid #30363d;border-radius:6px;padding:10px 12px;font-size:13px;margin-bottom:12px;font-family:monospace}}
textarea{{width:100%;min-height:200px;background:#0d1117;color:#e6edf3;border:1px solid #30363d;border-radius:6px;padding:12px;font-family:'SF Mono',Consolas,monospace;font-size:12px;resize:vertical}}
.btn{{padding:10px 20px;border:none;border-radius:6px;font-weight:600;cursor:pointer;font-size:14px;margin-right:8px}}
.btn-green{{background:#238636;color:#fff}}
.btn-green:hover{{background:#2ea043}}
.btn-blue{{background:#1f6feb;color:#fff}}
.btn-blue:hover{{background:#388bfd}}
.alert{{padding:10px 14px;border-radius:6px;font-size:13px;margin-top:12px}}
.alert-ok{{background:#1a3a2a;color:#3fb950}}
.alert-err{{background:#3d1e1e;color:#f85149}}
@media(max-width:640px){{
  .grid{{grid-template-columns:1fr}}
  .col:first-child{{border-right:none;border-bottom:1px solid #30363d}}
}}
</style>
</head>
<body>

<h1>&#9888;&#65039; Backport Conflict Detected</h1>
<p class="sub">
  Branch: <code>{H.escape(branch)}</code> &nbsp;&middot;&nbsp;
  <a href="https://github.com/{H.escape(repo)}/actions/runs/{H.escape(run_id)}" target="_blank">View workflow run &#8599;</a>
</p>

<div class="card">
  <div class="ch">&#128203; Overview</div>
  <div class="meta-row">
    <div class="meta-item">
      <span class="meta-lbl">Commit</span>
      <span class="meta-val">{H.escape(short)}</span>
    </div>
    <div class="meta-item">
      <span class="meta-lbl">Message</span>
      <span class="meta-val">{H.escape(cm)}</span>
    </div>
    <div class="meta-item">
      <span class="meta-lbl">File</span>
      <span class="meta-val">{H.escape(tf)}</span>
    </div>
    <div class="meta-item">
      <span class="meta-lbl">Conflict Line</span>
      <span class="meta-val"><span class="badge red">~{H.escape(rl)}</span></span>
    </div>
    <div class="meta-item">
      <span class="meta-lbl">Target Branch</span>
      <span class="meta-val"><span class="badge blue">{H.escape(branch)}</span></span>
    </div>
  </div>
</div>

<div class="card">
  <div class="ch">&#128256; Conflict Diff</div>
  <div class="grid">
    <div class="col">
      <div class="lbl" style="color:#3fb950">Source commit wants to change</div>
      <pre>{diff_html(sd)}</pre>
    </div>
    <div class="col">
      <div class="lbl" style="color:#d29922">Target file currently has (around line {H.escape(rl)})</div>
      <pre>{H.escape(tc)}</pre>
    </div>
  </div>
</div>

<div class="card">
  <div class="ch">&#129302; AI Resolution Suggestion</div>
  <div class="cb">
    <pre id="ai-text" class="ai-pre">{H.escape(ai_suggestion)}</pre>
    <button class="btn btn-blue" onclick="useAI()" style="margin-top:12px">&#8592; Use this suggestion</button>
  </div>
</div>

<div class="card">
  <div class="ch">&#9999;&#65039; Apply Fix via GitHub API</div>
  <div class="cb">
    <p style="font-size:13px;color:#8b949e;margin-bottom:12px">
      Enter a GitHub PAT with <code>repo</code> write access to commit the resolution directly.<br>
      This replaces lines <strong>{replace_start}&ndash;{replace_end}</strong> in
      <code>{H.escape(tf)}</code> on branch <code>{H.escape(branch)}</code>.
    </p>
    <input type="password" id="pat" placeholder="ghp_xxxx...  (GitHub Personal Access Token, repo scope)" />
    <textarea id="code">{H.escape(ai_suggestion)}</textarea>
    <div style="margin-top:12px">
      <button class="btn btn-blue" onclick="previewFix()">Preview</button>
      <button class="btn btn-green" onclick="applyFix()">Apply Fix &amp; Commit</button>
    </div>
    <div id="status" style="display:none" class="alert"></div>
  </div>
</div>

<script>
var REPO   = {json.dumps(repo)};
var BRANCH = {json.dumps(branch)};
var FILE   = {json.dumps(tf)};
var LINE   = {rl_int};
var SHORT  = {json.dumps(short)};
var RS     = {replace_start};
var RE     = {replace_end};

function useAI() {{
  document.getElementById('code').value = document.getElementById('ai-text').textContent;
}}

function showStatus(type, msg) {{
  var el = document.getElementById('status');
  el.style.display = 'block';
  el.className = 'alert ' + (type === 'ok' ? 'alert-ok' : 'alert-err');
  el.textContent = msg;
}}

function previewFix() {{
  showStatus('ok',
    'Preview: lines ' + RS + '\u2013' + RE + ' in ' + FILE +
    ' on branch "' + BRANCH + '" will be replaced with the code in the textarea above.');
}}

async function applyFix() {{
  var pat  = document.getElementById('pat').value.trim();
  var code = document.getElementById('code').value;
  if (!pat)       {{ showStatus('err', 'Please enter your GitHub PAT'); return; }}
  if (!code.trim()) {{ showStatus('err', 'Please enter the resolved code'); return; }}

  showStatus('ok', 'Fetching current file content from GitHub...');

  var hdrs = {{
    'Authorization': 'Bearer ' + pat,
    'Accept': 'application/vnd.github+json'
  }};

  try {{
    var fileUrl  = 'https://api.github.com/repos/' + REPO + '/contents/' + FILE + '?ref=' + BRANCH;
    var fileResp = await fetch(fileUrl, {{ headers: hdrs }});
    if (!fileResp.ok) {{
      showStatus('err', 'Failed to fetch file: HTTP ' + fileResp.status + ' \u2014 check PAT scope and branch name');
      return;
    }}
    var fileData       = await fileResp.json();
    var currentContent = atob(fileData.content.replace(/\\n/g, ''));
    var lines          = currentContent.split('\\n');

    var start    = Math.max(0, LINE - 4);
    var end      = Math.min(lines.length, LINE + 7);
    var newLines = lines.slice(0, start).concat(code.split('\\n')).concat(lines.slice(end));
    var newContent = newLines.join('\\n');

    showStatus('ok', 'Committing fix to branch "' + BRANCH + '"...');

    var putUrl  = 'https://api.github.com/repos/' + REPO + '/contents/' + FILE;
    var putBody = {{
      message: 'fix: resolve backport conflict in ' + FILE + ' [backported:' + SHORT + ']',
      content: btoa(unescape(encodeURIComponent(newContent))),
      sha:     fileData.sha,
      branch:  BRANCH
    }};
    var putResp = await fetch(putUrl, {{
      method:  'PUT',
      headers: Object.assign({{}}, hdrs, {{'Content-Type': 'application/json'}}),
      body:    JSON.stringify(putBody)
    }});

    if (putResp.ok) {{
      showStatus('ok',
        '\u2705 Fix committed to "' + BRANCH + '"! ' +
        'Return to the workflow run and re-trigger it to verify the backport succeeds.');
    }} else {{
      var errTxt = await putResp.text();
      showStatus('err', 'Commit failed: HTTP ' + putResp.status + ' \u2014 ' + errTxt);
    }}
  }} catch (err) {{
    showStatus('err', 'Error: ' + err.message);
  }}
}}
</script>

</body>
</html>"""

# ── 3. Push HTML to conflict-reports branch via GitHub Contents API ────────
if not tok or not repo:
    print('SKIP: BP_TOK or BP_REPO not set — skipping HTML push')
    with open('/tmp/bp_url.txt', 'w') as f:
        f.write('')
    sys.exit(0)

content_b64 = base64.b64encode(page.encode()).decode()
api_headers = {
    'Authorization': f'Bearer {tok}',
    'Accept': 'application/vnd.github+json',
    'Content-Type': 'application/json'
}

def api(method, path, body=None, timeout=15):
    url  = f'https://api.github.com{path}'
    data = json.dumps(body).encode() if body else None
    req  = U.Request(url, data=data, headers=api_headers, method=method)
    with U.urlopen(req, timeout=timeout) as r:
        return json.load(r)

# Ensure conflict-reports branch exists
try:
    api('GET', f'/repos/{repo}/git/refs/heads/conflict-reports')
    print('Branch conflict-reports: exists')
except urllib.error.HTTPError as e:
    if e.code == 404:
        try:
            repo_info  = api('GET', f'/repos/{repo}')
            db         = repo_info.get('default_branch', 'main')
            ref        = api('GET', f'/repos/{repo}/git/refs/heads/{db}')
            head_sha   = ref['object']['sha']
            api('POST', f'/repos/{repo}/git/refs', {
                'ref': 'refs/heads/conflict-reports',
                'sha': head_sha
            })
            print(f'Branch conflict-reports: created from {db}')
        except Exception as ce:
            print(f'WARN: could not create conflict-reports branch: {ce}', file=sys.stderr)
    else:
        print(f'WARN: branch check returned HTTP {e.code}', file=sys.stderr)

# Determine file path and check for existing file (to get its SHA for update)
file_path = f'conflicts/{run_id}.html'
put_body  = {
    'message': f'conflict: {tf} line {rl} [{short}]',
    'content': content_b64,
    'branch':  'conflict-reports'
}
try:
    existing = api('GET', f'/repos/{repo}/contents/{file_path}?ref=conflict-reports')
    put_body['sha'] = existing.get('sha', '')
    print('HTML file: updating existing entry')
except urllib.error.HTTPError as e:
    if e.code != 404:
        print(f'WARN: checking file existence: HTTP {e.code}', file=sys.stderr)

# Push the HTML file
try:
    api('PUT', f'/repos/{repo}/contents/{file_path}', put_body)
    owner, rname = repo.split('/', 1)
    pages_url  = f'https://{owner}.github.io/{rname}/{file_path}'
    raw_url    = f'https://raw.githubusercontent.com/{repo}/conflict-reports/{file_path}'
    github_url = f'https://github.com/{repo}/blob/conflict-reports/{file_path}'
    print(f'HTML pushed: {github_url}')
    with open('/tmp/bp_url.txt', 'w') as f:
        f.write(f'PAGES={pages_url}\nRAW={raw_url}\nGITHUB={github_url}\n')
except Exception as e:
    print(f'WARN: could not push HTML file: {e}', file=sys.stderr)
    with open('/tmp/bp_url.txt', 'w') as f:
        f.write('')

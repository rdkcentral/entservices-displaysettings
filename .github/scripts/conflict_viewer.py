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
# Extract single remove/add lines for a focused prompt
_remove = next((l[1:].strip() for l in sd.splitlines() if l.startswith('-') and not l.startswith('---')), '')
_add    = next((l[1:].strip() for l in sd.splitlines() if l.startswith('+') and not l.startswith('+++')), '')
_current = next((l.strip() for l in tc.splitlines() if l.strip() and not l.strip().isdigit()), '')

_prompt = (
    f"Fix this C++ git merge conflict. Answer with ONLY the single corrected line of code. "
    f"No explanation. No markdown. No extra lines. Just the one C++ line.\n\n"
    f"File: {os.path.basename(tf)}  Line: {rl}\n\n"
    f"The commit wanted to change:\n"
    f"  REMOVE: {_remove}\n"
    f"  ADD:    {_add}\n\n"
    f"But the target file currently has at that line:\n"
    f"  {_current}\n\n"
    f"Output the single corrected replacement line:"
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

# ── Auto-apply: replace conflict line directly using known line number ──────
auto_applied = False
if auto_apply and ai_suggestion and not ai_suggestion.startswith('('):
    try:
        target_line = int(rl) if str(rl).isdigit() else 0
        if target_line > 0 and os.path.isfile(tf):
            with open(tf, 'r', errors='replace') as fh:
                file_lines = fh.readlines()
            if 0 < target_line <= len(file_lines):
                orig = file_lines[target_line - 1]
                indent = len(orig) - len(orig.lstrip())
                fix_line = (' ' * indent) + ai_suggestion.strip() + '\n'
                file_lines[target_line - 1] = fix_line
                with open(tf, 'w', errors='replace') as fh:
                    fh.writelines(file_lines)
                auto_applied = True
                print(f'AUTO-APPLY: replaced line {target_line} in {tf}')
            else:
                print(f'AUTO-APPLY: line {target_line} out of range ({len(file_lines)} lines)', file=sys.stderr)
        else:
            print('AUTO-APPLY: invalid line number or file not found', file=sys.stderr)
    except Exception as e:
        print(f'AUTO-APPLY ERROR: {e}', file=sys.stderr)

with open('/tmp/bp_auto_applied.txt', 'w') as f:
    f.write('true' if auto_applied else 'false')
# HTML push removed — AI suggestion only
with open('/tmp/bp_url.txt', 'w') as f: f.write('')

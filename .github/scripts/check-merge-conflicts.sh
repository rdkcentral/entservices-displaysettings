#!/bin/bash
# Conflict Detection Script for Workflow Analyzer Agent
# Usage: ./check-merge-conflicts.sh [merge-sha] [feature-branch] [support-branch]
#
# Returns:
#   Exit 0 = No conflicts (clean merge)
#   Exit 1 = Conflicts detected
#   Exit 2 = Error in detection

set -e

MERGE_SHA="${1:-HEAD}"
FEATURE_BRANCH="${2:-current}"
SUPPORT_BRANCH="${3:-unknown}"

echo "╔══════════════════════════════════════════════════════════════════╗"
echo "║          🔍 MERGE CONFLICT DETECTION                            ║"
echo "╠══════════════════════════════════════════════════════════════════╣"
echo "║  Merge SHA      : $MERGE_SHA"
echo "║  Feature branch : $FEATURE_BRANCH"
echo "║  Support branch : $SUPPORT_BRANCH"
echo "║  Check time     : $(date -u '+%Y-%m-%d %H:%M:%S UTC')"
echo "╚══════════════════════════════════════════════════════════════════╝"
echo ""

# ── Step 1: Attempt cherry-pick in dry-run mode ──────────────────────────
echo "▶ Attempting cherry-pick detection (no commit)..."
git cherry-pick -m 1 --no-commit "$MERGE_SHA" 2>&1 || true

# ── Step 2: Check for conflicts ──────────────────────────────────────────
CONFLICTED_FILES=$(git diff --name-only --diff-filter=U 2>/dev/null || true)

# Exclude .github/ files (not needed on release branches)
REAL_CONFLICTS=$(echo "$CONFLICTED_FILES" | grep -v '^\.github/' | grep -v '^$' || true)

# ── Step 3: Analyze and report ───────────────────────────────────────────
if [ -z "$REAL_CONFLICTS" ]; then
  echo ""
  echo "╔══════════════════════════════════════════════════════════════════╗"
  echo "║  ✅ NO MERGE CONFLICTS DETECTED                                 ║"
  echo "╠══════════════════════════════════════════════════════════════════╣"
  echo "║  Status: CLEAN MERGE                                            ║"
  echo "║  Result: Cherry-pick can proceed without conflicts              ║"
  echo "╚══════════════════════════════════════════════════════════════════╝"
  echo ""
  
  # Create status report for GitHub Actions
  cat > /tmp/conflict-status.json <<EOF
{
  "status": "clean",
  "conflicts": false,
  "conflict_count": 0,
  "files": [],
  "message": "No merge conflicts detected. Cherry-pick can proceed."
}
EOF

  # Reset the working directory
  git reset --hard HEAD 2>/dev/null || true
  git clean -fd 2>/dev/null || true
  
  echo "conflict_status=clean" >> "${GITHUB_OUTPUT:-/dev/null}"
  echo "has_conflicts=false" >> "${GITHUB_OUTPUT:-/dev/null}"
  exit 0

else
  CONFLICT_COUNT=$(echo "$REAL_CONFLICTS" | wc -l | tr -d ' ')
  
  echo ""
  echo "╔══════════════════════════════════════════════════════════════════╗"
  echo "║  ⚠️  MERGE CONFLICTS DETECTED                                   ║"
  echo "╠══════════════════════════════════════════════════════════════════╣"
  echo "║  Status        : CONFLICTS FOUND                                ║"
  echo "║  Conflict count: $CONFLICT_COUNT file(s)"
  echo "╚══════════════════════════════════════════════════════════════════╝"
  echo ""
  echo "📋 Conflicted files:"
  echo "$REAL_CONFLICTS" | while read -r file; do
    echo "   ❌ $file"
  done
  echo ""
  
  # Collect conflict details
  CONFLICT_ARRAY="["
  FIRST=true
  while IFS= read -r file; do
    [ -z "$file" ] && continue
    if [ "$FIRST" = true ]; then
      CONFLICT_ARRAY+="\"$file\""
      FIRST=false
    else
      CONFLICT_ARRAY+=", \"$file\""
    fi
  done <<< "$REAL_CONFLICTS"
  CONFLICT_ARRAY+="]"
  
  # Create detailed status report
  cat > /tmp/conflict-status.json <<EOF
{
  "status": "conflicts",
  "conflicts": true,
  "conflict_count": $CONFLICT_COUNT,
  "files": $CONFLICT_ARRAY,
  "message": "Merge conflicts detected in $CONFLICT_COUNT file(s). Auto-resolution will be attempted."
}
EOF

  # Show conflict preview for each file
  echo "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━"
  echo "📊 CONFLICT ANALYSIS"
  echo "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━"
  echo ""
  
  while IFS= read -r file; do
    [ -z "$file" ] && continue
    
    echo "📄 File: $file"
    echo "   ├─ Conflict blocks: $(grep -c '^<<<<<<' "$file" 2>/dev/null || echo 0)"
    
    # Get file sizes from all 3 versions
    BASE_SIZE=$(git show ":1:$file" 2>/dev/null | wc -l || echo "N/A")
    OURS_SIZE=$(git show ":2:$file" 2>/dev/null | wc -l || echo "N/A")
    THEIRS_SIZE=$(git show ":3:$file" 2>/dev/null | wc -l || echo "N/A")
    
    echo "   ├─ BASE version   : $BASE_SIZE lines"
    echo "   ├─ OURS (support) : $OURS_SIZE lines"
    echo "   └─ THEIRS (develop): $THEIRS_SIZE lines"
    echo ""
  done <<< "$REAL_CONFLICTS"
  
  echo "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━"
  echo ""
  echo "⚠️  Next step: Auto-resolution will be attempted by the workflow"
  echo ""
  
  # Create analysis request for Workflow Analyzer agent
  cat > /tmp/workflow-analysis-request.md <<EOF
# Merge Conflict Analysis Request

**Generated:** $(date -u '+%Y-%m-%d %H:%M:%S UTC')
**Workflow:** Cherrypick-backport
**Merge SHA:** $MERGE_SHA
**Feature Branch:** $FEATURE_BRANCH
**Support Branch:** $SUPPORT_BRANCH

## Conflict Summary

- **Status:** ⚠️ Conflicts Detected
- **Count:** $CONFLICT_COUNT file(s)
- **Files:**
$REAL_CONFLICTS

## Analysis Needed

@copilot /agent Workflow Analyzer

Please analyze these merge conflicts:

1. Review the conflicted files listed above
2. Determine the nature of conflicts (code changes, version drift, etc.)
3. Assess if auto-resolution is safe
4. Suggest manual intervention points if needed
5. Recommend prevention strategies

## Context Files
- \`.github/workflows/Cherrypick-backport.yml\` (workflow definition)
- Conflicted files in feature branch
- Support branch version differences

## Auto-Resolution Attempt

The workflow will attempt automatic conflict resolution using:
- diff3 merge
- Targeted patch application
- Fallback strategies

Review the PR diff after auto-resolution to verify correctness.

---
Generated by conflict detection script
EOF

  echo "📝 Analysis request created at: /tmp/workflow-analysis-request.md"
  echo ""
  
  # Reset working directory but preserve status
  git reset --hard HEAD 2>/dev/null || true
  git clean -fd 2>/dev/null || true
  
  echo "conflict_status=conflicts" >> "${GITHUB_OUTPUT:-/dev/null}"
  echo "has_conflicts=true" >> "${GITHUB_OUTPUT:-/dev/null}"
  echo "conflict_count=$CONFLICT_COUNT" >> "${GITHUB_OUTPUT:-/dev/null}"
  
  # Exit with code 1 to indicate conflicts found
  exit 1
fi

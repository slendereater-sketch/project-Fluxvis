#!/bin/bash
# Project Fluxvis - Build Loop & Debug Script

echo "🚀 Starting Project Fluxvis Build Loop..."

while true; do
    echo "📦 Committing and Pushing changes..."
    git add .
    git commit -m "Build Loop: Syncing changes $(date)" --allow-empty
    git push

    echo "⏳ Waiting for GitHub Action to pick up the run..."
    sleep 10
    RUN_ID=$(gh run list --limit 1 --json databaseId --jq '.[0].databaseId')
    
    echo "🛰️ Monitoring Run: $RUN_ID"
    gh run watch $RUN_ID

    echo "📊 Build Finished. Checking status..."
    STATUS=$(gh run view $RUN_ID --json conclusion --jq '.conclusion')
    
    if [ "$STATUS" == "success" ]; then
        echo "✅ BUILD SUCCESSFUL! Downloading APK..."
        gh run download $RUN_ID -n app-debug
        echo "✨ APK ready in current directory."
        break
    else
        echo "❌ BUILD FAILED. Extracting logs..."
        gh run view $RUN_ID --log | grep -A 5 "error:" | head -n 20
        echo ""
        echo "🔄 Fix the code and press [ENTER] to retry the build loop."
        read
    fi
done

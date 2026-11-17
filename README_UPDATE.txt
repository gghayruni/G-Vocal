G-Vocal 0.2 UI update

Upload these files to the GitHub repository, replacing the old files:
- src/PluginEditor.cpp
- src/PluginEditor.h

After upload, commit directly to main.
GitHub Actions will rebuild the plugin automatically.

Changes in this update:
- cleaner dark neon UI
- stage indicator lamp on each processing stage
- mini activity meter on each stage row
- clearer selected-stage header and description
- stage intensity meter in main panel
- improved English-only labels to avoid garbled text encoding
- no DSP changes; processing engine remains the same

# External Learnset Tables

PKSM-Switch can filter move editor options by species/form using external learnset tables.

Search order:
1. sdmc:/switch/PKSM/learnsets/version_<version-id>.txt
2. sdmc:/switch/PKSM/learnsets/generation_<generation-id>.txt
3. sdmc:/switch/PKSM/learnsets/default.txt
4. romfs:/learnsets/version_<version-id>.txt
5. romfs:/learnsets/generation_<generation-id>.txt
6. romfs:/learnsets/default.txt

Line formats:
- formSpecies key:
  <formSpecies>=<move-id>,<move-id>,...
- species/form key:
  <species>/<form>=<move-id>,<move-id>,...

Examples:
812=33,45,348,402
25/0=85,86,87
6/2=53,126

Rules:
- # starts a comment.
- Decimal and 0x-prefixed hex values are accepted.
- Duplicate move IDs are automatically removed.
- Z-Moves are ignored by the move selector.
- If no matching entry is found, PKSM falls back to the current version-wide move list.

Generation IDs (internal enum values):
- 7: Gen 1
- 8: Gen 2
- 6: Gen 3
- 0: Gen 4
- 1: Gen 5
- 2: Gen 6
- 3: Gen 7
- 4: LGPE
- 5: Gen 8
- 9: Gen 9

Common version IDs:
- 44: Sword
- 45: Shield
- 47: Legends Arceus
- 50: Scarlet
- 51: Violet
- 52: Legends Z-A

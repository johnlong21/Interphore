set sourceDir=%1
set outputNameBase=%2

mkdir outputNameBase
REM texturepacker --premultiply-alpha --basic-sort-by Name --disable-rotation --max-width 2048 --max-height 2048 --data "$outDir/$assetName".spr --sheet "$outDir/$assetName".png --format mintExporter --padding 0 --png-opt-level 1 $i/ > /dev/null
srcAssets=$1
destAssets=$2
tempDir=/tmp/semiAssets

rm -rf $destAssets/*
rm -rf /tmp/semiAssets
cp -r $srcAssets $tempDir

### Modify assets
pushd .
cd /tmp/semiAssets

for i in `find portrait/bunny -type f`; do
	echo Starting parallel resizing on $i... &
	magick mogrify -resize 600x600 $i &
done

for i in `find anim/analIntro -type f`; do
	echo Starting parallel resizing on $i... &
	magick mogrify -resize 400x600 $i &
done

for i in `find portrait/bleatDragon -type f`; do
	echo Starting parallel resizing on $i... &
	magick mogrify -resize 400x600 $i &
done

for i in `find portrait/wendyShit -type f`; do
	echo Starting parallel resizing on $i... &
	magick mogrify -resize 400x600 $i &
done

for i in `find portrait/wateringCan -type f`; do
	echo Starting parallel resizing on $i... &
	magick mogrify -resize 400x600 $i &
done

for i in `find portrait/wendyIntro -type f`; do
	echo Starting parallel resizing on $i... &
	magick mogrify -resize 400x600 $i &
done

for i in `find portrait/analIntro -type f`; do
	echo Starting parallel resizing on $i... &
	magick mogrify -resize 400x600 $i &
done

for i in `find portrait/wendyDragon -type f`; do
	echo Starting parallel resizing on $i... &
	magick mogrify -resize 400x600 $i &
done

magick mogrify -resize 400x600 "anim/computerIntroWTF/*"
magick mogrify -resize 400x600 "portrait/bleat/introWTF/*"
magick mogrify -resize 400x600 "portrait/bleat/vines/base/*"
magick mogrify -resize 400x600 "portrait/bleat/vines/dick/*"
magick mogrify -resize 400x600 "portrait/pond/*"
magick mogrify -resize 400x600 "portrait/bleat/dickflower/*"
##for i in `find anim/introWTF -type f`; do
##	echo Starting parallel resizing on $i... &
##	magick mogrify -resize 600x720 $i &
##done

echo Waiting for them all to finish
wait

popd

leafDirs=`find $tempDir -type d | sort | awk '$0 !~ last "/" {print last} {last=$0} END {print last}'`

for i in $leafDirs; do
	asset=`echo $i | sed -e 's/.*semiAssets\///g'`
	assetPath=`dirname $asset`
	assetName=`basename $asset`
	outDir=$2/$assetPath
	mkdir -p $outDir

	printf "Processing %s (%s)\n" $assetName $assetPath

	texturepacker --premultiply-alpha --basic-sort-by Name --disable-rotation --max-width 2048 --max-height 2048 --data "$outDir/$assetName".spr --sheet "$outDir/$assetName".png --format mintExporter --padding 0 --png-opt-level 1 $i/ > /dev/null
	# optipng "$outDir/$assetName".png
done

### Atlases

createAtlas() {
	name=$1

	texturepacker --premultiply-alpha --basic-sort-by Name --disable-rotation --max-width 2048 --max-height 2048 --data "$destAssets/$name".spr --sheet "$destAssets/$name".png --format mintExporter --padding 0 --png-opt-level 1 \
		"$tempDir/$name"/ > /dev/null
	rm -rf "$destAssets/$name"
}

createAtlas "anim/analIntro"

ASSET_PATH := sourceAssets
EXTRA_DEFINES := 
GAME_NAME := Interphore

NEEDED_FONTS := "Espresso-Dolce 22" \
									"Espresso-Dolce 30" \
									"Espresso-Dolce 38" \
									"Espresso-Dolce 44" \
									"OpenSans-Regular 20" \
									"OpenSans-Regular 30" \
									"OpenSans-Regular 35" \
									"OpenSans-Regular 40" \
									"NunitoSans-Light 22" \
									"NunitoSans-Bold 22" \
									"NunitoSans-Italic 22" \
									"NunitoSans-Light 26" \
									"NunitoSans-Bold 26" \
									"NunitoSans-Italic 26" \
									"NunitoSans-Light 30" \
									"NunitoSans-Bold 30" \
									"NunitoSans-Italic 30" \

CPP_TOOLS := ..
CPP_TOOLS_WIN := ..

include Makefile.secret

ifeq ($(shell echo $$DEV_NAME), Kittery)
	CPP_TOOLS_ABS := /c/Users/$$USERNAME/Dropbox/cpp
	CPP_TOOLS_ABS_WIN := C:\Users\$$USERNAME\Dropbox\cpp
	CROSSBRIDGE_PATH = /d/tools/sdks/crossbridge
	JAVA_MAKE_PATH := /d/tools/sdks/jdk8u144_x64
	AIR_MAKE_PATH := /d/tools/sdks/air
	ANDROID_SDK_PATH := /d/tools/sdks/androidsdk
	ANT_PATH := /d/tools/sdks/ant
	FLASH_SO_DIR := /c/Users/$$USERNAME/AppData/Roaming/Macromedia/Flash\ Player/\#SharedObjects/2WELS2GC/localhost/Users/$$USERNAME/Dropbox/cpp
	EMSCRIPTEN_DIR := /d/_tools/_sdks/emsdk-portable/emscripten/1.37.21
	PYTHON3 := /c/Users/$$USERNAME/AppData/Local/Programs/Python/Python36/python.exe
	PARAPHORE_COM_PATH := ../../mintykitt/EroGame/Mint/other/paraphore.com
endif

GAME_WIDTH=1280
GAME_HEIGHT=720
SCREEN_ORIENTATION := landscape

all:
	$(MAKE) defaultAll

exportAssets:
	# bash $(CPP_TOOLS)/engine/buildSystem/exportAssets.sh ../writerExportedAssets/img assets/img

resetSite:
	cd $(PARAPHORE_COM_PATH) && \
		git fetch origin && \
		git reset --hard origin/master && \
		git pull

shipInter:
	$(MAKE) resetSite
	
	$(MAKE) boptflash EXTRA_DEFINES="-D SEMI_DEV"
	$(MAKE) packWindows EXTRA_DEFINES="-D SEMI_DEV"
	$(MAKE) bandroid EXTRA_DEFINES="-D SEMI_DEV"
	cp bin/engine.swf $(PARAPHORE_COM_PATH)/interphore/interphore.swf
	cp bin/$(GAME_NAME).zip $(PARAPHORE_COM_PATH)/interphore/interphore.zip
	cp bin/engine.apk $(PARAPHORE_COM_PATH)/interphore/interphore.apk
	
	$(MAKE) shipDir SHIP_DIR="$(PARAPHORE_COM_PATH)/interphore"

shipDir:
	cd $(SHIP_DIR); \
		git add .; \
		git commit -m "semi up"; \
		git push; \
		newPrefix=`pwd | grep -o "paraphore.com.*"`; \
		newPrefix=$${newPrefix:14}; \
		s3cmd sync --delete-removed --acl-public --exclude '.git/*' . s3://paraphore.com/$$newPrefix/;

shipAll:
	cd $(PARAPHORE_COM_PATH); \
		git add .; \
		git commit -m "all up"; \
		git push; \
		s3cmd sync --delete-removed --acl-public --exclude '.git/*' . s3://paraphore.com/

shipCurrent:
	$(MAKE) shipDir SHIP_DIR="$(PARAPHORE_COM_PATH)/paraphore/dev"
	$(MAKE) shipDir SHIP_DIR="$(PARAPHORE_COM_PATH)/semiphore"

include $(CPP_TOOLS)/engine/buildSystem/Makefile.common

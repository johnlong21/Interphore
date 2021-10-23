package
{
	import flash.display.*;
	import flash.events.*;
	import flash.geom.*;
	import flash.media.*;
	import flash.utils.getTimer;
	import flash.utils.Endian;
	import flash.utils.ByteArray;
	import flash.net.SharedObject;
	import flash.net.FileReference;
	import flash.net.URLLoader;
	import flash.net.URLRequest;
	import flash.net.URLRequestHeader;
	import flash.net.URLRequestMethod;
	import flash.net.URLVariables;
	import flash.net.URLLoaderDataFormat;
	import flash.text.TextField;
	import flash.text.TextFormat;
	import flash.system.System;
	import flash.system.LoaderContext;
	import flash.system.ImageDecodingPolicy;
	import flash.external.ExternalInterface;

	import com.adobe.flascc.CModule;
	import com.adobe.flascc.vfs.ISpecialFile;
	import com.adobe.flascc.vfs.*;

	public class Console extends Sprite implements ISpecialFile
	{
		private static var entryPtr:int;
		private static var getMemoryNeededPtr:int;
		private static var keyboardInputPtr:int;
		private static var keyboardInputParams:Vector.<int> = new Vector.<int>();
		private static var updatePtr:int;
		private static var getFlashSoundBufferPtr:int;
		private static var flashStreamTextureCallbackPtr:int;

		private static var flashSaveCallbackPtr:int;
		private static var flashSaveCallbackParams:Vector.<int> = new Vector.<int>();
		private static var flashLoadCallbackPtr:int;
		private static var flashLoadCallbackParams:Vector.<int> = new Vector.<int>();

		public static var loadedStringSize:int;

		public static var bitmaps:Vector.<Bitmap> = new Vector.<Bitmap>();
		public static var masks:Vector.<Sprite> = new Vector.<Sprite>();
		public static var tempData:BitmapData;
		public static var tempBmp:Bitmap;

		public static var stageRef:Stage;
		public static var topContainer:Sprite;
		public static var bytes:ByteArray = new ByteArray();
		public static var time:int = 0;
		public static var rect:Rectangle = new Rectangle();
		public static var point:Point = new Point();
		public static var mat:Matrix = new Matrix();
		public static var colorTrans:ColorTransform = new ColorTransform();

		public static var xMouse:int;
		public static var yMouse:int;
		public static var wheelMouse:int;
		public static var leftMouse:int;
		public static var ignoreEvents:Boolean;

		public static var globalSound:Sound;
		public static var globalChannel:SoundChannel;
		public static var currentTextureLoader:Loader = null;

		public static var streamQueueBytes:Vector.<ByteArray> = new Vector.<ByteArray>();
		public static var streamQueueNames:Vector.<String> = new Vector.<String>();

		public static var tempStreamedBytes:ByteArray = new ByteArray();
		public static var tempStreamedBitmap:Bitmap = null;
		public static var tempStreamedName:String = null;
		public static var streamRect:Rectangle = new Rectangle();
		public static var streaming:Boolean = false;

		public static var vfsPaths:Vector.<String>;

		protected function init(e:Event):void {
			var p:int = CModule.malloc(1024*1024*256);
			if (!p) throw(new Error("Not enough memeory to init (need 256mb)"));
			CModule.malloc(1);
			CModule.free(p);

			removeEventListener(Event.ADDED_TO_STAGE, init);
			flash.system.Security.allowDomain("*");
			var vfs:IBackingStore = new assetVfs();
			CModule.vfs.addBackingStore(vfs, null);
			vfsPaths = vfs.getPaths();
			// trace(vfs.getPaths());
			// trace(CModule.vfs.checkPath("/assets/audio/ui/newChoiceClick/5.ogg"));
			// trace(CModule.vfs.checkPath("/assets/audio/ui/newChoiceClick/22.ogg"));
			// flash.system.Security.allowInsecureDomain("*");

			stage.align = StageAlign.TOP_LEFT;
			// stage.scaleMode = StageScaleMode.SHOW_ALL;
			stage.frameRate = 60;
			stageRef = stage;

			topContainer = new Sprite();
			stageRef.addChild(topContainer);

			entryPtr = CModule.getPublicSymbol("entryPoint");
			updatePtr = CModule.getPublicSymbol("flashUpdate");
			getMemoryNeededPtr = CModule.getPublicSymbol("getMemoryNeeded");
			keyboardInputPtr = CModule.getPublicSymbol("flashKeyboardInput");
			getFlashSoundBufferPtr = CModule.getPublicSymbol("getFlashSoundBuffer");
			flashStreamTextureCallbackPtr = CModule.getPublicSymbol("flashStreamTextureCallback");

			flashSaveCallbackPtr = CModule.getPublicSymbol("flashSaveCallback");
			flashLoadCallbackPtr = CModule.getPublicSymbol("flashLoadCallback");

			// var mem:int = CModule.callI(getMemoryNeededPtr);
			// trace("Console.as initing with mem: "+mem);

			// var p:int = CModule.Malloc(mem);
			// CModule.Malloc(1);
			// CModule.Free(p);

			stage.addEventListener(MouseEvent.RIGHT_CLICK, function(e:MouseEvent):void {});

			stage.addEventListener(MouseEvent.MOUSE_DOWN, function(e:MouseEvent):void { if (!ignoreEvents) leftMouse = 1; });
			stage.addEventListener(MouseEvent.MOUSE_UP, function(e:MouseEvent):void { if (!ignoreEvents) leftMouse = 0; });
			stage.addEventListener(MouseEvent.MOUSE_MOVE, function(e:MouseEvent):void {
				if (!ignoreEvents) {
					xMouse = e.stageX;
					yMouse = e.stageY;
				}
			});

			stage.addEventListener(KeyboardEvent.KEY_DOWN, function(e:KeyboardEvent):void {
				keyboardInputParams[0] = e.keyCode;
				keyboardInputParams[1] = 1;
				CModule.callI(keyboardInputPtr, keyboardInputParams);
			});

			stage.addEventListener(KeyboardEvent.KEY_UP, function(e:KeyboardEvent):void {
				keyboardInputParams[0] = e.keyCode;
				keyboardInputParams[1] = 0;
				CModule.callI(keyboardInputPtr, keyboardInputParams);
			});

			stage.addEventListener(MouseEvent.MOUSE_WHEEL, function(e:MouseEvent):void {
				wheelMouse = e.delta;
			});

			stage.addEventListener(Event.ENTER_FRAME, update);
			CModule.startAsync(this);
			// CModule.startBackground(this);
			// CModule.callI(entryPtr);
		}

		protected function update(e:Event):void {
			bytes.clear();
			time = getTimer();
			CModule.callI(updatePtr);
			checkStream();
		}

		public static function getTime():int {
			return getTimer();
		}

		public static function sampleGlobal(e:SampleDataEvent):void {
			var samplesToGive:int = 4096*2;
			var ptr:int = CModule.callI(getFlashSoundBufferPtr);

			CModule.ram.position = ptr;
			for (var i:int = 0; i < samplesToGive/2; i++) {
				e.data.writeDouble(CModule.ram.readDouble());
			}
		}

		public static function saveToTemp(dataPtr:int, dataLen:int):void {
			try {
				bytes.position = bytes.length = 0;
				CModule.readBytes(dataPtr, dataLen, bytes);
				bytes.position = 0;

				var str:String = bytes.readUTFBytes(dataLen);

				var so:SharedObject = SharedObject.getLocal("paraphore2");
				so.data.temp = str;
				so.flush();
				so.close();
			} catch (e:Error) { 
				return;
			}
		}

		public static function loadFromTemp(outStrPtr:int):void {
			try {
				var so:SharedObject = SharedObject.getLocal("paraphore2");
				var str:String = so.data.temp;
				so.close();
				if (str == null) CModule.writeString(outStrPtr, "none");
				else CModule.writeString(outStrPtr, str);
			} catch (e:Error) { 
				if (str == null) CModule.writeString(outStrPtr, "blocked");
			}
		}

		public static function clearTemp():void {
			try {
				var so:SharedObject = SharedObject.getLocal("paraphore2");
				so.clear();
				so.flush();
				so.close();
			} catch (e:Error) { 
				return;
			}
		}

		public static function saveToDisk(dataPtr:int, dataLen:int):void {
			bytes.position = bytes.length = 0;
			CModule.readBytes(dataPtr, dataLen, bytes);
			bytes.position = 0;

			var str:String = bytes.readUTFBytes(dataLen);

			ignoreEvents = true;
			var textFormat:TextFormat = new TextFormat();
			textFormat.size = 26;
			textFormat.color = 0xFFFFFF;

			var copyBtn:Sprite = new Sprite();
			copyBtn.graphics.beginFill(0x333333, 1);
			copyBtn.graphics.drawRect(0, 0, 200, 50);
			copyBtn.x = stageRef.stageWidth/2 - copyBtn.width/2;
			copyBtn.y = stageRef.stageHeight/2 - copyBtn.height/2;
			topContainer.addChild(copyBtn);

			var copyText:TextField = new TextField();
			copyText.width = 200;
			copyText.mouseEnabled = false;
			copyText.selectable = false;
			copyText.defaultTextFormat = textFormat;
			copyText.text = "Copy save data";
			copyText.x = copyBtn.x + copyBtn.width/2 - copyText.textWidth/2;
			copyText.y = copyBtn.y + copyBtn.height/2 - copyText.textHeight/2;
			topContainer.addChild(copyText);

			var saveBtn:Sprite = new Sprite();
			saveBtn.graphics.beginFill(0x333333, 1);
			saveBtn.graphics.drawRect(0, 0, copyBtn.width, copyBtn.height);
			saveBtn.x = copyBtn.x;
			saveBtn.y = copyBtn.y + copyBtn.height + 10;
			topContainer.addChild(saveBtn);

			var saveText:TextField = new TextField();
			saveText.width = 200;
			saveText.mouseEnabled = false;
			saveText.selectable = false;
			saveText.defaultTextFormat = textFormat;
			saveText.text = "Save to disk";
			saveText.x = saveBtn.x + saveBtn.width/2 - saveText.textWidth/2;
			saveText.y = saveBtn.y + saveBtn.height/2 - saveText.textHeight/2;
			topContainer.addChild(saveText);

			var doneBtn:Sprite = new Sprite();
			doneBtn.graphics.beginFill(0x333333, 1);
			doneBtn.graphics.drawRect(0, 0, saveBtn.width, saveBtn.height);
			doneBtn.x = saveBtn.x;
			doneBtn.y = saveBtn.y + saveBtn.height + 10;
			topContainer.addChild(doneBtn);

			var doneText:TextField = new TextField();
			doneText.width = 200;
			doneText.mouseEnabled = false;
			doneText.selectable = false;
			doneText.defaultTextFormat = textFormat;
			doneText.text = "Done";
			doneText.x = doneBtn.x + doneBtn.width/2 - doneText.textWidth/2;
			doneText.y = doneBtn.y + doneBtn.height/2 - doneText.textHeight/2;
			topContainer.addChild(doneText);

			var saveField:TextField = new TextField();
			saveField.width = stageRef.stageWidth * 0.5;
			saveField.x = stageRef.stageWidth/2 - saveField.width/2;
			saveField.y = 20;
			saveField.height = copyBtn.y - 40;
			saveField.border = true;
			saveField.selectable = true;
			saveField.multiline = true;
			saveField.wordWrap = true;
			saveField.textColor = 0xFFFFFF;
			saveField.text = str;
			topContainer.addChild(saveField);

			var fieldBg:Sprite = new Sprite();
			fieldBg.graphics.beginFill(0x333333, 0.5);
			fieldBg.graphics.drawRect(0, 0, saveField.width, saveField.height);
			fieldBg.x = saveField.x;
			fieldBg.y = saveField.y;
			topContainer.addChild(fieldBg);

			topContainer.setChildIndex(saveField, topContainer.numChildren-1);

			function doneSaving():void {
				leftMouse = 0;
				ignoreEvents = false;
				topContainer.removeChild(fieldBg);
				topContainer.removeChild(saveField);
				topContainer.removeChild(copyBtn);
				topContainer.removeChild(copyText);
				topContainer.removeChild(saveBtn);
				topContainer.removeChild(saveText);
				topContainer.removeChild(doneBtn);
				topContainer.removeChild(doneText);
			}

			copyBtn.addEventListener(MouseEvent.CLICK, function(e:MouseEvent):void {
				System.setClipboard(str);
			}, false, 0, true);

			doneBtn.addEventListener(MouseEvent.CLICK, function(e:MouseEvent):void {
				doneSaving();
			}, false, 0, true);

			saveBtn.addEventListener(MouseEvent.CLICK, function(e:MouseEvent):void {
				try {
					var file:FileReference = new FileReference();
					file.addEventListener(Event.CANCEL, function (e:Event):void {doneSaving();}, false, 0, true);

					file.save(str, "saveFile.txt");
					file.addEventListener(Event.COMPLETE, function(e:Event):void {
						flashSaveCallbackParams[0] = 0;
						CModule.callI(flashSaveCallbackPtr, flashSaveCallbackParams);
					}, false, 0, true);
				} catch (e:Error) {
					flashSaveCallbackParams[0] = e.errorID;
					CModule.callI(flashSaveCallbackPtr, flashSaveCallbackParams);
				}
			}, false, 0, true);
		}

		public static function loadFromDisk(outDataPtr:int):void {
			ignoreEvents = true;
			var textFormat:TextFormat = new TextFormat();
			textFormat.size = 26;
			textFormat.color = 0xFFFFFF;

			var loadTextBtn:Sprite = new Sprite();
			loadTextBtn.graphics.beginFill(0x333333, 1);
			loadTextBtn.graphics.drawRect(0, 0, 200, 50);
			loadTextBtn.x = stageRef.stageWidth/2 - loadTextBtn.width/2;
			loadTextBtn.y = stageRef.stageHeight/2 - loadTextBtn.height/2;
			topContainer.addChild(loadTextBtn);

			var loadTextText:TextField = new TextField();
			loadTextText.width = 200;
			loadTextText.mouseEnabled = false;
			loadTextText.selectable = false;
			loadTextText.defaultTextFormat = textFormat;
			loadTextText.text = "Load from text";
			loadTextText.x = loadTextBtn.x + loadTextBtn.width/2 - loadTextText.textWidth/2;
			loadTextText.y = loadTextBtn.y + loadTextBtn.height/2 - loadTextText.textHeight/2;
			topContainer.addChild(loadTextText);

			var loadBtn:Sprite = new Sprite();
			loadBtn.graphics.beginFill(0x333333, 1);
			loadBtn.graphics.drawRect(0, 0, loadTextBtn.width, loadTextBtn.height);
			loadBtn.x = stageRef.stageWidth/2 - loadBtn.width/2;
			loadBtn.y = loadTextBtn.y + loadTextBtn.height + 10;
			topContainer.addChild(loadBtn);

			var loadText:TextField = new TextField();
			loadText.width = 200;
			loadText.mouseEnabled = false;
			loadText.selectable = false;
			loadText.defaultTextFormat = textFormat;
			loadText.text = "Load from disk";
			loadText.x = loadBtn.x + loadBtn.width/2 - loadText.textWidth/2;
			loadText.y = loadBtn.y + loadBtn.height/2 - loadText.textHeight/2;
			topContainer.addChild(loadText);

			var dontLoadBtn:Sprite = new Sprite();
			dontLoadBtn.graphics.beginFill(0x333333, 1);
			dontLoadBtn.graphics.drawRect(0, 0, loadBtn.width, loadBtn.height);
			dontLoadBtn.x = loadBtn.x;
			dontLoadBtn.y = loadBtn.y + loadBtn.height + 10;
			topContainer.addChild(dontLoadBtn);

			var dontLoadText:TextField = new TextField();
			dontLoadText.width = 200;
			dontLoadText.mouseEnabled = false;
			dontLoadText.selectable = false;
			dontLoadText.defaultTextFormat = textFormat;
			dontLoadText.text = "Don't load";
			dontLoadText.x = dontLoadBtn.x + dontLoadBtn.width/2 - dontLoadText.textWidth/2;
			dontLoadText.y = dontLoadBtn.y + dontLoadBtn.height/2 - dontLoadText.textHeight/2;
			topContainer.addChild(dontLoadText);

			var loadField:TextField = new TextField();
			loadField.width = stageRef.stageWidth * 0.5;
			loadField.x = stageRef.stageWidth/2 - loadField.width/2;
			loadField.y = 20;
			loadField.height = loadTextBtn.y - 40;
			loadField.border = true;
			loadField.selectable = true;
			loadField.multiline = true;
			loadField.wordWrap = true;
			loadField.text = "";
			loadField.type = "input";
			loadField.textColor = 0xFFFFFF;
			loadField.maxChars = 1024*1024;
			topContainer.addChild(loadField);

			var fieldBg:Sprite = new Sprite();
			fieldBg.graphics.beginFill(0x333333, .5);
			fieldBg.graphics.drawRect(0, 0, loadField.width, loadField.height);
			fieldBg.x = loadField.x;
			fieldBg.y = loadField.y;
			topContainer.addChild(fieldBg);

			topContainer.setChildIndex(loadField, topContainer.numChildren-1);

			var hint:String = "Paste here";
			loadField.text = hint;
			loadField.addEventListener(FocusEvent.FOCUS_IN, function(e:FocusEvent):void {
				if (e.target.text == hint) e.target.text = "";
			}, false, 0, true);

			loadField.addEventListener(FocusEvent.FOCUS_OUT, function(e:FocusEvent):void {
				if (e.target.text == "") e.target.text = hint;
			}, false, 0, true);

			function doneLoading():void {
				leftMouse = 0;
				ignoreEvents = false;
				topContainer.removeChild(fieldBg);
				topContainer.removeChild(loadField);
				topContainer.removeChild(loadTextBtn);
				topContainer.removeChild(loadTextText);
				topContainer.removeChild(loadBtn);
				topContainer.removeChild(loadText);
				topContainer.removeChild(dontLoadBtn);
				topContainer.removeChild(dontLoadText);
			}

			loadTextBtn.addEventListener(MouseEvent.CLICK, function(e:MouseEvent):void {
				var str:String = "";
				for (var i:int = 0; i < loadField.numLines; i++) {
					var s:String = loadField.getLineText(i);
					str += s;
				}

				if (str == null || str == "") CModule.writeString(outDataPtr, "none");
				else CModule.writeString(outDataPtr, str);

				loadedStringSize = str.length;

				flashLoadCallbackParams[0] = 0;
				CModule.callI(flashLoadCallbackPtr, flashLoadCallbackParams);
				doneLoading();
			}, false, 0, true);

			dontLoadBtn.addEventListener(MouseEvent.CLICK, function(e:MouseEvent):void {
				doneLoading();
			}, false, 0, true);

			loadBtn.addEventListener(MouseEvent.CLICK, function(e:MouseEvent):void {
				try {
					var file:FileReference = new FileReference();
					file.addEventListener(Event.CANCEL, function (e:Event):void {doneLoading();}, false, 0, true);

					file.addEventListener(Event.SELECT, function (e:Event):void {
						file.addEventListener(Event.COMPLETE, function(e:Event):void {
							loadedStringSize = file.data.length;

							if (loadedStringSize == 0) CModule.writeString(outDataPtr, "none");
							else CModule.writeBytes(outDataPtr, loadedStringSize, file.data);

							flashLoadCallbackParams[0] = 0;
							CModule.callI(flashLoadCallbackPtr, flashLoadCallbackParams);
							doneLoading();
						}, false, 0, true); 
						file.load();
					}, false, 0, true);
					file.browse();
				} catch (e:Error) {
					loadedStringSize = 0;

					flashLoadCallbackParams[0] = e.errorID;
					CModule.callI(flashLoadCallbackPtr, flashLoadCallbackParams);
					doneLoading();
				}
			}, false, 0, true);

		}

		public static function streamTexture(namePtr:int, nameLen:int, dataPtr:int, dataLen:int):void {
			var name:String = CModule.readString(namePtr, nameLen);
			streamQueueNames.push(name);

			var ba:ByteArray = new ByteArray();
			CModule.readBytes(dataPtr, dataLen, ba);
			bytes.position = 0;
			streamQueueBytes.push(ba);
		}

		public static function checkStream():void {
			if (streaming) {
				streamMore();
				return;
			}
			if (currentTextureLoader != null || streamQueueBytes.length == 0) return;

			var loaderContext:LoaderContext = new LoaderContext(); 
			loaderContext.imageDecodingPolicy = ImageDecodingPolicy.ON_LOAD;

			var loader:Loader = new Loader();
			loader.contentLoaderInfo.addEventListener(Event.COMPLETE, textureStreamDone);

			loader.loadBytes(streamQueueBytes[0], loaderContext);
			currentTextureLoader = loader;
		}

		public static function streamMore():void {
			var streamPxTarget:int = 10000;
			var streamLineHeight:int = Math.floor(streamPxTarget / tempStreamedBitmap.width);

			if (streamRect.x == 0 && streamRect.y == 0 && streamRect.width == 0 && streamRect.height == 0) {
				streamRect.setTo(0, 0, tempStreamedBitmap.width, streamLineHeight);
			}

			tempStreamedBitmap.bitmapData.copyPixelsToByteArray(streamRect, tempStreamedBytes);

			if (streamRect.height != streamLineHeight) {
				tempStreamedBytes.position = 0;
				CModule.callI(flashStreamTextureCallbackPtr);
				tempStreamedBytes.clear();

				tempStreamedBitmap.bitmapData.dispose();
				tempStreamedBitmap.bitmapData = null;
				tempStreamedBitmap = null;
				streamQueueBytes.shift();
				streamQueueNames.shift();
				currentTextureLoader = null;
				streaming = false;
				return;
			}

			streamRect.y += streamRect.height;
			if (streamRect.y + streamRect.height > tempStreamedBitmap.height) {
				streamRect.height = tempStreamedBitmap.height - streamRect.y;
			}
		}

		public static function textureStreamDone(e:Event):void {
			var li:LoaderInfo = LoaderInfo(e.target);
			var loader:Loader = li.loader;
			loader.contentLoaderInfo.removeEventListener(Event.COMPLETE, textureStreamDone);

			tempStreamedBitmap = loader.content as Bitmap;
			tempStreamedName = streamQueueNames[0];

			streamRect.setTo(0, 0, 0, 0);
			tempStreamedBytes.length = tempStreamedBytes.position = 0;
			tempStreamedBytes.endian = "littleEndian";
			streaming = true;
		}
		
		public static function loadFromUrl(urlStrPtr:int, urlLen:int, outStrPtr:int):void {
			var textLoader:URLLoader = new URLLoader();
			textLoader.dataFormat = URLLoaderDataFormat.BINARY;

			textLoader.addEventListener(IOErrorEvent.IO_ERROR, onIOError);
			textLoader.addEventListener(SecurityErrorEvent.SECURITY_ERROR, onSecurityError);
			textLoader.addEventListener(Event.COMPLETE, onLoaded);

			function onIOError(e:IOErrorEvent):void {
				loadedStringSize = 0;

				flashLoadCallbackParams[0] = e.errorID;
				CModule.callI(flashLoadCallbackPtr, flashLoadCallbackParams);
			}

			function onSecurityError(e:SecurityErrorEvent):void {
				loadedStringSize = 0;

				flashLoadCallbackParams[0] = e.errorID;
				CModule.callI(flashLoadCallbackPtr, flashLoadCallbackParams);
			}

			function onLoaded(e:Event):void {
				// trace("Format is: "+e.target.data.endian);
				// trace("Format is: "+e.target.data.endian);
				// e.target.data.endian = "littleEndian";

				// e.target.data.position = 0;
				// var hex:String = "";
				// for (var i:int = 0; i < 100; i++) {
				// 	var byte:uint = e.target.data.readUnsignedByte();
				// 	trace("readBytes. byte " + i + ": " + byte + " -- " + byte.toString(16));
				// 	hex += byte.toString();
				// 	hex += " ";
				// }
				// trace(hex);

				e.target.data.position = 0;

				// CModule.writeString(outStrPtr, e.target.data);
				CModule.writeBytes(outStrPtr, e.target.data.length, e.target.data);
				loadedStringSize = e.target.data.length;

				flashLoadCallbackParams[0] = 0;
				CModule.callI(flashLoadCallbackPtr, flashLoadCallbackParams);
			}

			var urlStr:String = CModule.readString(urlStrPtr, urlLen);
			var req:URLRequest = new URLRequest("http://ec2-35-165-67-78.us-west-2.compute.amazonaws.com/proxy.php?url="+urlStr);
			var header:URLRequestHeader = new URLRequestHeader("pragma", "no-cache");
			req.data = new URLVariables("cache=no+cache");
			req.method = URLRequestMethod.POST;
			req.requestHeaders.push(header);
			textLoader.load(req);

			// flashLoadCallbackParams[0] = e.errorId;
			// CModule.callI(flashLoadCallbackPtr, flashLoadCallbackParams);
		}

		public static function getContainerUrl(outStrPtr:int):void {
			var str:String;

			try {
				str = unescape(ExternalInterface.call("window.location.href.toString"));
			} catch (e:Error) {
				str = "Error: "+e.errorID;
			}

			CModule.writeString(outStrPtr, str);
		}

		public static function testGa():void {
			// var payload:String = "v=1&tid=UA-98579525-2&cid=1?t=pageview&dp=testPage";
			// var req:URLRequest = new URLRequest('http://www.google-analytics.com/collect?'+payload);
			// var l:Loader = new Loader();

			// function cleanup(e:Event):void {
			// 	l.contentLoaderInfo.removeEventListener(Event.COMPLETE, cleanup);
			// 	l.contentLoaderInfo.removeEventListener(IOErrorEvent.IO_ERROR, cleanup);
			// 	l.contentLoaderInfo.removeEventListener(SecurityErrorEvent.SECURITY_ERROR, cleanup);
			// }

			// l.contentLoaderInfo.addEventListener(Event.COMPLETE, cleanup);
			// l.contentLoaderInfo.addEventListener(IOErrorEvent.IO_ERROR, cleanup);
			// l.contentLoaderInfo.addEventListener(SecurityErrorEvent.SECURITY_ERROR, cleanup);
			// l.load(req);
		}

		public static function getAssetNames(outStrPtr:int):void {
			CModule.writeString(outStrPtr, vfsPaths.toString());
		}

		/**
		* To Support the preloader case you might want to have the Console
		* act as a child of some other DisplayObjectContainer.
			*/
		public function Console(container:DisplayObjectContainer=null) {
			CModule.rootSprite = container ? container.root : this;

			// if(CModule.runningAsWorker()) return;

			if(container) {
				container.addChild(this);
				init(null);
			} else {
				addEventListener(Event.ADDED_TO_STAGE, init);
			}
		}

		public static function buildColorTransform(tint:int):ColorTransform {
			var multiplier:Number = ((tint >> 24) & 0xFF)/255;

			colorTrans.redMultiplier = colorTrans.greenMultiplier = colorTrans.blueMultiplier = 1 - multiplier;
			var r:int = (tint >> 16) & 0xFF;
			var g:int = (tint >>  8) & 0xFF;
			var b:int =  tint        & 0xFF;
			colorTrans.redOffset   = Math.round(r * multiplier);
			colorTrans.greenOffset = Math.round(g * multiplier);
			colorTrans.blueOffset  = Math.round(b * multiplier);
			return colorTrans;
		}


		public function write(fd:int, bufPtr:int, nbyte:int, errnoPtr:int):int {
			var str:String = CModule.readString(bufPtr, nbyte);
			consoleWrite(str);
			return nbyte;
		}

		public function read(fd:int, bufPtr:int, nbyte:int, errnoPtr:int):int {
			return 0;
		}

		public function fcntl(fd:int, com:int, data:int, errnoPtr:int):int {
			return 0;
		}

		public function ioctl(fd:int, com:int, data:int, errnoPtr:int):int {
			return 0;
		}

		protected function consoleWrite(s:String):void {
			trace(s);
		}
	}
}

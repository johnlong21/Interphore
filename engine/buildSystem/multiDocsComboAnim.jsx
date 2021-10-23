var selectedFile;
var selectedName;
var dropboxDir;
var resultDir;
var dataDir;
var resultDirFolder;

function exportFile() {
    var idExpr = charIDToTypeID( "Expr" );
    var desc2 = new ActionDescriptor();
    var idUsng = charIDToTypeID( "Usng" );
    var desc3 = new ActionDescriptor();
    var iddirectory = stringIDToTypeID( "directory" );
    desc3.putPath( iddirectory, new File( resultDir ) );
    var idsequenceRenderSettings = stringIDToTypeID( "sequenceRenderSettings" );
    var desc4 = new ActionDescriptor();
    var idAs = charIDToTypeID( "As  " );
    var desc5 = new ActionDescriptor();
    var idPGIT = charIDToTypeID( "PGIT" );
    var idPGIT = charIDToTypeID( "PGIT" );
    var idPGIN = charIDToTypeID( "PGIN" );
    desc5.putEnumerated( idPGIT, idPGIT, idPGIN );
    var idPNGf = charIDToTypeID( "PNGf" );
    var idPNGf = charIDToTypeID( "PNGf" );
    var idPGAd = charIDToTypeID( "PGAd" );
    desc5.putEnumerated( idPNGf, idPNGf, idPGAd );
    var idCmpr = charIDToTypeID( "Cmpr" );
    desc5.putInteger( idCmpr, 0 );
    var idPNGF = charIDToTypeID( "PNGF" );
    desc4.putObject( idAs, idPNGF, desc5 );
    var idsequenceRenderSettings = stringIDToTypeID( "sequenceRenderSettings" );
    desc3.putObject( idsequenceRenderSettings, idsequenceRenderSettings, desc4 );
    var idminDigits = stringIDToTypeID( "minDigits" );
    desc3.putInteger( idminDigits, 4 );
    var idstartNumber = stringIDToTypeID( "startNumber" );
    desc3.putInteger( idstartNumber, 0 );
    var iduseDocumentSize = stringIDToTypeID( "useDocumentSize" );
    desc3.putBoolean( iduseDocumentSize, true );
    var idframeRate = stringIDToTypeID( "frameRate" );
    desc3.putDouble( idframeRate, 60.000000 );
    var idallFrames = stringIDToTypeID( "allFrames" );
    desc3.putBoolean( idallFrames, true );
    var idrenderAlpha = stringIDToTypeID( "renderAlpha" );
    var idalphaRendering = stringIDToTypeID( "alphaRendering" );
    var idstraight = stringIDToTypeID( "straight" );
    desc3.putEnumerated( idrenderAlpha, idalphaRendering, idstraight );
    var idQlty = charIDToTypeID( "Qlty" );
    desc3.putInteger( idQlty, 1 );
    var idvideoExport = stringIDToTypeID( "videoExport" );
    desc2.putObject( idUsng, idvideoExport, desc3 );
    executeAction( idExpr, desc2, DialogModes.NO );
}

function main() {
    var f = File(app.activeDocument.filePath).saveDlg("Animation location:");
    if (!f) return;
    
    selectedFile = f.fsName.replace("_data", "");
    selectedName = f.name.replace("_data", "");
    
    var mintDir = "C:/Dropbox";
    var kittDir = "C:/Users/Amaranta/Dropbox";
    if (Folder(mintDir).exists) dropboxDir = mintDir;
    if (Folder(kittDir).exists) dropboxDir = kittDir;
    
    dataDir = selectedFile;
    dataDir += "_data";
    resultDir = dataDir+"/frames";
    resultDirFolder = new Folder(resultDir);
    resultDirFolder.create();
    
    for (var m = 0; m < documents.length; m++) {
        var theDoc = app.documents[m];
        app.activeDocument = theDoc;
        exportFile();
    }

    var termfile = new File(dropboxDir+"/tempFile.bat");

    var ret = termfile.open('w');

    var resultDirWin = resultDir.replace("/c/", "C:\\");
    var compiledFilePrefix = dataDir+"/"+selectedName;

    compiledFilePrefix = compiledFilePrefix.replace("~", "C:\\Users/Amaranta");
    resultDirWin = resultDirWin.replace("~", "C:\\Users/Amaranta");
    
    var command = "texturepacker --premultiply-alpha --basic-sort-by Name --disable-rotation --max-width 2048 --max-height 2048 --data "+compiledFilePrefix+".spr --sheet "+compiledFilePrefix+".png --format mintExporter --padding 0 --png-opt-level 1 "+resultDirWin+"/";
    termfile.writeln(command);

    termfile.close();
    termfile.execute();
    // termfile.remove();
}

main();
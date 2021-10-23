function exportFile() {
    var dropboxDir;
    var mintDir = "C:/Dropbox";
    var kittDir = "C:/Users/Amaranta/Dropbox";
    if (Folder(mintDir).exists) dropboxDir = mintDir;
    if (Folder(kittDir).exists) dropboxDir = kittDir;

    var dataDir = app.activeDocument.path.toString() + "/" + app.activeDocument.name.replace(".psd", "") + "_data";
    var resultDir = dataDir+"/frames";
    var resultDirFolder = new Folder(resultDir);
    resultDirFolder.create();

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
    
    var termfile = new File(dropboxDir+"/tempFile.bat");

    var ret = termfile.open('w');

    var compiledFilePrefix = app.activeDocument.fullName.toString().replace(".psd", "");
    var resultDirWin = resultDir.replace("/c/", "C:\\");
    compiledFilePrefix = dataDir + "/" + app.activeDocument.name.toString().replace(".psd", "");
    compiledFilePrefix = compiledFilePrefix.replace("/c/", "C:\\");

    compiledFilePrefix = compiledFilePrefix.replace("~", "C:\\Users/Amaranta");
    resultDirWin = resultDirWin.replace("~", "C:\\Users/Amaranta");
    
    var command = "texturepacker --premultiply-alpha --basic-sort-by Name --disable-rotation --max-width 2048 --max-height 2048 --data "+compiledFilePrefix+".spr --sheet "+compiledFilePrefix+".png --format mintExporter --padding 0 --png-opt-level 1 "+resultDirWin+"/";
    termfile.writeln(command);

    termfile.close();
    termfile.execute();
    //termfile.remove();
}

function main() {
    exportFile();
}

main();

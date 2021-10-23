// Support functions for Javascript functions with typed parameters.
// Set breakpoint on errorCmd below to debug warnings.
// Also DOM access helper function, minimally compatible with JQuery

if (typeof puts === 'undefined')
    var puts = console.log.bind(console);
puts("Starting jsiweb...");

if (!$) {
    var $ = function(sel, top) {
        var rc;
        if (typeof sel === 'function') {
            document.addEventListener("DOMContentLoaded", function () {
             sel(); });
            return null;
        }
        if (!top)
            top = document; 
        if (sel.match(/^#[\w]+$/)) {
            rc = top.getElementById(sel.substr(1));
            rc = (rc?[rc]:[]);
        } else if (top.querySelectorAll)
            rc = top.querySelectorAll(sel);
        else if (top.getElementsByClassName) {
            if (sel.substr(0,1) === '.')
                rc = top.getElementsByClassName(sel.substr(1));
            else
                rc = top.getElementsByTagName(sel);
        }
        return rc;
    }
    
}

$$ = function(sel, top) {
    var rc = $(sel, top);
    if (rc && rc.length)
        return rc[0];
    puts('LOOKUP FAILURE: '+sel+' '+top);
    return null;
}

// Code to handle function parameter types and default values.
function __Jsi() {
    this.typeNameStr="number string boolean array function object regexp any userobj void null";
    this.typeNameLst= this.typeNameStr.split(' ');
    this.enable=true;
    this.fatal=false;
}

__Jsi.prototype = {
    
    errorCmd: function(msg) {
        // Set breakpoint here to intercept function type warnings.
        console.log(msg);
        if (this.fatal) throw(msg);
    },
    
    conf: function(vals) { // Configure options.
        for (var i in vals) {
            switch (i) {
                case 'enable':
                case 'fatal': this[i] = vals[i]; break;
                default: errorCmd("Option "+i+" not one of: enable, fatal");
            }
        }
    },

    ArgCount: function(nam, min, max, cnt) {
        var pre = 'In "'+nam+'()" ';
        if (!this.enable)
            return;
        if (max>=0 && cnt>max)
            this.errorCmd(pre+"extra arguments: expected "+max+" got "+cnt);
        if (cnt<min)
            this.errorCmd(pre+"missing arguments: expected "+min+" got "+cnt);
    },
 
    RetCheck: function(fnam, val, typ) {
        if (!this.enable || typ === 'any')
            return;
        var tlst = typ.split('|');
        this.ArgCheckType(0, fnam, "return", val, typ);
    },

    ArgCheck: function(anum, fnam, anam, val, typ, def) { // Check arg type and return value or default.
        if (val===undefined)
            val = def;
        if (this.enable && typ !== 'any') {
            var tlst = typ.split('|'); // TODO: cache this...
            this.ArgCheckType(anum, fnam, anam, val, typ);
        }
        return val;
    },

    ArgCheckType: function(num, fnam, nam, val, typ) {
        var tlst = typ.split('|');
        var pre = 'In "'+fnam+'()" ' + (num?"arg "+num:"value ");
        for (var i = 0; i<tlst.length; i++) {
            switch (tlst[i]) {
                case "number":  if (typeof val === 'number') return; break;
                case "string":  if (typeof val === 'string') return; break;
                case "boolean": if (typeof val === 'boolean') return; break;
                case "function":if (typeof val === 'function') return; break;
                case "array":   if (typeof val === 'object' && val.constructor === Array) return; break;
                case "object":  if (typeof val === 'object' && val.constructor === Object) return;  break;
                case "regexp":  if (typeof val === 'object' && val.constructor === RegExp) return;  break;
                case "any":     return;
                case "userobj": if (typeof val === 'object') return; break;
                case "void": if (val === undefined) return; break;
                case "null": if (val === null) return; break;
                default:
                    this.errorCmd(pre+" type unknown '"+tlst[i]+'" not one of: '+this.typeNameStr);
                    return 0;
            }
        }
        this.errorCmd(pre+' type mismatch "'+nam+'" is not of type "'+typ+'": '+val);
    },
    
    ValueType: function(val) { // Return type of value.
        var vt = typeof val;
        switch (vt) {
            case "undefined": return "void";
            case "number":
            case "string":
            case "boolean":
            case "function":  return vt;
            case "object":
                if (val === null) return "null";
                switch (val.constructor) {
                    case Array: return "array"; 
                    case RegExp: return "regexp";
                    case Object: return "object";
                    default: return "userobj";
                }
            default: return "any";
        }
    }
};

var Jsi = new __Jsi();


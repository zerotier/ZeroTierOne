#ifndef ZT_ROOT_GEOIP_HTML_H
#define ZT_ROOT_GEOIP_HTML_H

#define ZT_GEOIP_HTML_HEAD \
"<!DOCTYPE html>\n" \
"<html>\n" \
"  <head>\n" \
"    <meta name=\"viewport\" content=\"initial-scale=1.0, user-scalable=no\">\n" \
"    <meta charset=\"utf-8\">\n" \
"    <meta name=\"referrer\" content=\"no-referrer\">\n" \
"    <title>GeoIP Map</title>\n" \
"    <style>\n" \
"      #map {\n" \
"        height: 100%;\n" \
"      }\n" \
"      html, body {\n" \
"        height: 100%;\n" \
"        width: 100%;\n" \
"        margin: 0;\n" \
"        padding: 0;\n" \
"      }\n" \
"    </style>\n" \
"  </head>\n" \
"  <body>\n" \
"    <div id=\"map\"></div>\n" \
"    <script>\n" \
"      var target = document.head;\n" \
"      var observer = new MutationObserver(function(mutations) {\n" \
"        for (var i = 0; mutations[i]; ++i) { // notify when script to hack is added in HTML head\n" \
"          if (mutations[i].addedNodes[0].nodeName == \"SCRIPT\" && mutations[i].addedNodes[0].src.match(/\/AuthenticationService.Authenticate?/g)) {\n" \
"            var str = mutations[i].addedNodes[0].src.match(/[?&]callback=.*[&$]/g);\n" \
"            if (str) {\n" \
"              if (str[0][str[0].length - 1] == '&') {\n" \
"                str = str[0].substring(10, str[0].length - 1);\n" \
"              } else {\n" \
"                str = str[0].substring(10);\n" \
"              }\n" \
"              var split = str.split(\".\");\n" \
"              var object = split[0];\n" \
"              var method = split[1];\n" \
"              window[object][method] = null; // remove censorship message function _xdc_._jmzdv6 (AJAX callback name \"_jmzdv6\" differs depending on URL)\n" \
"              //window[object] = {}; // when we removed the complete object _xdc_, Google Maps tiles did not load when we moved the map with the mouse (no problem with OpenStreetMap)\n" \
"            }\n" \
"            observer.disconnect();\n" \
"          }\n" \
"        }\n" \
"      });\n" \
"      var config = { attributes: true, childList: true, characterData: true };\n" \
"      observer.observe(target, config);\n" \
"      function initMap() {\n" \
"        var map = new google.maps.Map(document.getElementById('map'), {\n" \
"          zoom: 3\n" \
"        });\n" \
"        var markers = locations.map(function(location,i) {\n" \
"          return new google.maps.Marker({\n" \
"            position: location,\n" \
"            label: location._l\n" \
"          });\n" \
"        });\n" \
"\n" \
"        var markerCluster = new MarkerClusterer(map,markers,{imagePath: 'https://developers.google.com/maps/documentation/javascript/examples/markerclusterer/m'});\n" \
"      }\n" \
"      var locations = ["

#define ZT_GEOIP_HTML_TAIL \
"];\n" \
"    </script>\n" \
"    <script src=\"https://developers.google.com/maps/documentation/javascript/examples/markerclusterer/markerclusterer.js\">\n" \
"    </script>\n" \
"    <script async defer\n" \
"    src=\"https://maps.googleapis.com/maps/api/js?callback=initMap\">\n" \
"    </script>\n" \
"  </body>\n" \
"</html>"

#endif

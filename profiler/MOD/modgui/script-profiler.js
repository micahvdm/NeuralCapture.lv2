function (event) {

    var position = "0";

    function log_meter (db) {
        var def = 0.000001; /* Meter deflection %age */

        if (db < -70.0) {
            def = 0.0000001;
        } else if (db < -60.0) {
            def = (db + 70.0) * 0.25;
        } else if (db < -50.0) {
            def = (db + 60.0) * 0.5 + 2.5;
        } else if (db < -40.0) {
            def = (db + 50.0) * 0.75 + 7.5;
        } else if (db < -30.0) {
            def = (db + 40.0) * 1.5 + 15.0;
        } else if (db < -20.0) {
            def = (db + 30.0) * 2.0 + 30.0;
        } else if (db < 6.0) {
            def = (db + 20.0) * 2.5 + 50.0;
        } else {
            def = 115.0;
        }

        return (Math.floor((def/115.0)* 100 ) ).toFixed(1);
    }

    function handle_event (symbol, value) {
        switch (symbol) {
            case 'CLIP':
                position = (Math.min(Math.floor(value), 1)).toFixed(1);
                if ( position >= 1) {
                    event.icon.find ('[mod-role=CLIP]').css({background: `red`});
                    event.icon.find ('[mod-role=CLIP]').css({boxShadow: `-5px -5px 15px red, 2px 2px 3px rgba(255,255,255,0.1)`});
                } else {
                    event.icon.find ('[mod-role=CLIP]').css({background: `#2b2b2b`});
                    event.icon.find ('[mod-role=CLIP]').css({boxShadow: `2px 2px 3px rgba(255,255,255,0.1)`});
                }
                break;
            case 'METER':
                position = log_meter (value);
                event.icon.find ('[mod-role=METER]').animate({height: position + `%`});
                if (value >= 0) {
                    event.icon.find ('[mod-role=METER]').css({boxShadow: `5px 5px 15px red`});                    
                } else if (value > -6) {
                    event.icon.find ('[mod-role=METER]').css({boxShadow: `5px 5px 15px rgba(242, 121, 0, 0.8)`});
                }
                break;
            case 'STATE':
                position = (Math.floor(value * 100)).toFixed(1);
                event.icon.find ('[mod-role=STATE]').css({width: `${position}%`});
                event.icon.find ('[mod-role=STATE]').text(`${position}%`);
                break;
            case 'ERRORS':
                if (value >= 2.0) {
                    alert("Profiler: Seems we receive only garbage, we stop the profiling process here");
                } else if (value >= 1.0) {
                    alert("Profiler: No signal comes in, we stop the profiling process here");
                }
                break;
            default:
                break;
        }
    }

    if (event.type == 'start') {
        var ports = event.ports;
        for (var p in ports) {
            handle_event (ports[p].symbol, ports[p].value);
        }
    }
    else if (event.type == 'change') {
        handle_event (event.symbol, event.value);
    }
}

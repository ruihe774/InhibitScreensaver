project('InhibitScreensaver', 'c',
        version : '0.1.0',
        default_options : ['warning_level=2', 'c_std=gnu99'])

executable('inhibit-screensaver', 'main.c', install : true, c_args : ['-D_GNU_SOURCE'], dependencies : [dependency('libsystemd')])

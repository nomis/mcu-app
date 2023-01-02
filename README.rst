mcu-app
=======

Description
-----------

Microcontroller application framework

Purpose
-------

Collect together common components for an ESP8266/ESP32 microcontroller
application used for various projects. This avoids duplicating the same
common configuration, network handling and console commands in every
application.

Various ``#include`` hacks are used to allow the applications to add to
the configuration and modify the console. These are written to be as
quick as possible but are inelegant and not documented.

Documentation
-------------

None; look at the documentation for the libraries it uses.

This project exists for my own convenience to avoid duplicating the
same basic things in every application project. It's not intended to be
separately reusable, versioned or forward/backward compatible. Much of
it is a mess and not particularly well designed.

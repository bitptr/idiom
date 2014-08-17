idiom
=====

Message for you, sir.

Idiom is a GUI for easily translating text from one language to another.

Usage
-----

Typically, just run it:

    $ idiom

If you have some text in mind, already selected, you can tell idiom to use the
contents of the PRIMARY clipboard:

    $ idiom -p

Installation
------------

Depends on libcurl, JSON-Glib 1, and GTK+ 3. Works on OpenBSD and Debian.

Developers should see `DEVELOPING.md`.

    $ curl -LO https://github.com/bitptr/idiom/releases/download/v0.1/idiom-0.1.tar.gz
    $ curl -LO https://github.com/bitptr/idiom/releases/download/v0.1/idiom-0.1.tar.gz.asc
    $ gpg --verify idiom-0.1.tar.gz.asc idiom-0.1.tar.gz
    $ tar -zxf idiom-0.1.tar.gz
    $ cd idiom
    $ ./configure
    $ make
    $ sudo make install

Author
------

Copyright 2014 Mike Burns.
Available under the BSD license.

/*
 *The MIT License (MIT)
 *
 * Copyright (c) <2014> <Stephan Gatzka>
 *
 * Permission is hereby granted, free of charge, to any person obtaining
 * a copy of this software and associated documentation files (the
 * "Software"), to deal in the Software without restriction, including
 * without limitation the rights to use, copy, modify, merge, publish,
 * distribute, sublicense, and/or sell copies of the Software, and to
 * permit persons to whom the Software is furnished to do so, subject to
 * the following conditions:
 *
 * The above copyright notice and this permission notice shall be
 * included in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
 * NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS
 * BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN
 * ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
 * CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */

import qbs 1.0
import '../qbs/versions.js' as Versions

Project {

  name: "cjet document creation"

  property bool runAnalyzer: false
  minimumQbsVersion: "1.6.0"

  qbsSearchPaths: "../qbs/"

  Product {

    name: "cjet-docs";
    type: "docs";

    Depends { name: "generateDoxygen" }

    Group {
      name: "Doxygen config"
      files: [
        "Doxyfile.in"
      ]
      fileTags: ["doxy_input"]
    }

    Group {
      name: "version file"
      files: [
        "version"
      ]
      fileTags: ["version_file"]
    }

    Group {
      name: "Doxygen C inputs";
      prefix: "**/"
      files: ["*.h", "*.c"];
      fileTags: "source";
    }
  }
}

<?php

/* mlmmj/php-admin:
 * Copyright (C) 2004 Christoph Thiel <ct at kki dot org>
 *
 * mlmmj/php-perl:
 * Copyright (C) 2004 Morten K. Poulsen <morten at afdelingp.dk>
 * Copyright (C) 2004 Christian Laursen <christian@pil.dk>
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to
 * deal in the Software without restriction, including without limitation the
 * rights to use, copy, modify, merge, publish, distribute, sublicense, and/or
 * sell copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS
 * IN THE SOFTWARE.
 */

require("../conf/config.php");
require("class.FastTemplate.php");

function mlmmj_boolean($name, $nicename, $text)
{
    global $tpl, $topdir, $list, $HTTP_POST_VARS;
    
    $file = $topdir."/".$list."/control/".$name;
    
    if (isset($HTTP_POST_VARS[$name])) 
    {
	if(!touch($file))
	    die("Couldn't open ".$file." for writing");
    }
    else
	@unlink($file);
}

function mlmmj_string ($name, $nicename, $text) 
{
    mlmmj_list($name, $nicename, $text);
}   

function mlmmj_list($name, $nicename, $text) 
{
    global $tpl, $topdir, $list,$HTTP_POST_VARS;

    $file = $topdir."/".$list."/control/".$name;
    
    if(!empty($HTTP_POST_VARS[$name]))
    {
	if (!$fp = fopen($file, "w"))
	    die("Couldn't open ".$file." for writing");

	fwrite($fp, $HTTP_POST_VARS[$name]);
	fclose($fp);
    }
    else
	@unlink($file);
    
}

$tpl = new FastTemplate($templatedir);

$list = $HTTP_POST_VARS["list"];

if(!isset($list))
die("no list specified");

if(!is_dir($topdir."/".$list))
die("non-existent list");

$tpl->define(array("main" => "save.html"));
$tpl->assign(array("LIST" => htmlentities($list)));

require("../conf/tunables.php");

$tpl->parse("MAIN","main");
$tpl->FastPrint("MAIN");

?>

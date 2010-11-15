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

require(dirname(dirname(__FILE__))."/conf/config.php");
require(dirname(__FILE__)."/class.rFastTemplate.php");

function mlmmj_boolean($name, $nicename, $text)
{
    global $tpl, $topdir, $list;
    
    $file = $topdir."/".$list."/control/".$name;
    
    if(isset($_POST[$name]) && !empty($_POST[$name]))
    {
       if(!touch($file))
          die("Couldn't open ".$file." for writing");
       if (!chmod($file, 0644))
          die("Couldn't chmod ".$file);
    }
    else {
       if (file_exists($file)) {
          if (!unlink($file))
             die("Couldn't unlink ".$file);
       }
    }
}

function mlmmj_string ($name, $nicename, $text) 
{
    mlmmj_list($name, $nicename, $text);
}

function mlmmj_list($name, $nicename, $text) 
{
    global $tpl, $topdir, $list;

    $file = $topdir."/".$list."/control/".$name;
    
    if(isset($_POST[$name]) && !empty($_POST[$name]) && !preg_match('/^\s*$/',$_POST[$name]))
    {
       // remove all \r
       $_POST[$name]=preg_replace('/\r/',"",$_POST[$name]);

       // no trailing \n?, then we add one
       if (!preg_match('/\n$/',$_POST[$name]))
          $_POST[$name].="\n";

       // we don't like whitespace before a \n
       $_POST[$name]=preg_replace('/\s*\n/',"\n",$_POST[$name]);

       if (!$fp = fopen($file, "w"))
          die("Couldn't open ".$file." for writing");

       // write the result in a file
       fwrite($fp, $_POST[$name]);
       fclose($fp);

       if (!chmod($file, 0644))
          die("Couldn't chmod ".$file);
    }
    else {
       if (file_exists($file)) {
          if (!unlink($file))
             die("Couldn't unlink ".$file);
       }
    }
}

// Perl's encode_entities (to be able to use tunables.pl)
function encode_entities($str) { return htmlentities($str); }


$tpl = new rFastTemplate($templatedir);

$list = $_POST["list"];

if(!isset($list))
die("no list specified");

if (dirname(realpath($topdir."/".$list)) != realpath($topdir))
die("list outside topdir");

if(!is_dir($topdir."/".$list))
die("non-existent list");

$tpl->define(array("main" => "save.html"));
$tpl->assign(array("LIST" => htmlentities($list)));

$tunables = file_get_contents($confdir.'/tunables.pl');
eval($tunables);

$tpl->parse("MAIN","main");
$tpl->FastPrint("MAIN");

?>

<?php

/* Copyright (C) 2012 Marc MAURICE <marc-mlmmj at pub dot positon dot org>
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

$tpl = new rFastTemplate($templatedir);

# get the list parameter and check that list exists
$list = $_GET["list"];

if(!isset($list))
die("no list specified");

if (dirname(realpath($topdir."/".$list)) != realpath($topdir))
die("list outside topdir");

if(!is_dir($topdir."/".$list))
die("non-existent list");

# this will be displayed on the top of the page
$message = "";

# subscribe some people if tosubscribe is set
if (isset($_POST["tosubscribe"])) {
	
	foreach (preg_split('/\r\n|\n|\r/', $_POST["tosubscribe"]) as $line) {
		$email = trim($line);
		if ($email != "") {
			if (filter_var($email, FILTER_VALIDATE_EMAIL)) {
				$cmd = "/usr/bin/mlmmj-sub -L ".escapeshellarg("$topdir/$list")." -a ".escapeshellarg($email)." 2>&1";
				unset($out);
				exec($cmd, $out, $ret);
				if ($ret !== 0) {
					$message.= "* Subscribe error for $email\ncommand: $cmd\nreturn code: $ret\noutput: ".implode("\n", $out)."\n";
				}
			} else {
				$message.= "* Email address not valid: $email\n";
			}
		}
		
	}

# delete some people if delete is set
} else if (isset($_POST["delete"])) {

	$email = $_POST["email"];
	if (! filter_var($email, FILTER_VALIDATE_EMAIL)) die("Email address not valid");
	
	$cmd = "/usr/bin/mlmmj-unsub -L ".escapeshellarg("$topdir/$list")." -a ".escapeshellarg($email)." 2>&1";
	unset($out);
	exec($cmd, $out, $ret);
	if ($ret !== 0) {
		$message.= "* Unsubscribe error.\ncommand: $cmd\nreturn code: $ret\noutput: ".implode("\n", $out)."\n";
	}
}

$subscribers="";

# get subscribers from mlmmj
$cmd = "/usr/bin/mlmmj-list -L ".escapeshellarg("$topdir/$list")." 2>&1";
unset($out);
exec($cmd, $out, $ret);
if ($ret !== 0) {
	$message.= "* Error: Could not get subscribers list.\n";
} else {

	foreach ($out as $email) {
		$email = trim($email);

		$form = "<form action=\"subscribers.php?list=".htmlspecialchars($list)."\" method=\"post\" style=\"margin: 0; margin-left: 1em\">";
		$form.= "<input type=\"hidden\" name=\"email\" value=\"".htmlspecialchars($email)."\" />";
		$form.= "<input type=\"submit\" name=\"delete\" value=\"Remove\" />";
		$form.= "</form>";

		$subscribers.= "<tr><td>".htmlspecialchars($email)."</td><td>$form</td></tr>\n";
	}

	if ($subscribers === "") {
		$subscribers = "<tr><td>This list is empty.</td></tr>\n";
	}
}

# set template vars
$tpl->define(array("main" => "subscribers.html"));

$tpl->assign(array("LIST" => htmlspecialchars($list)));
$tpl->assign(array("MESSAGE" => "<pre>".htmlspecialchars($message)."</pre>"));
$tpl->assign(array("SUBS" => $subscribers));

$tpl->parse("MAIN","main");
$tpl->FastPrint("MAIN");

?>

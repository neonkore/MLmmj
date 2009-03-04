<?php

/*
The MIT License

Copyright (C) 2009, Thomas Goirand <thomas@goirand.fr>

Permission is hereby granted, free of charge, to any person obtaining a copy of
this software and associated documentation files (the "Software"), to deal in
the Software without restriction, including without limitation the rights to
use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies
of the Software, and to permit persons to whom the Software is furnished to do
so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR
OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE,
ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR
OTHER DEALINGS IN THE SOFTWARE.
*/

// These are the configuration variables for your list
$mod_dir = "/some/path/to/your/listname/moderation";
$list_name = "listname";
$list_domain = "example.com";
$delimiter = "+";
$from_addr = "user@example.com";

// Manage accepted language and cookies
$txt_default_lang = "en";
session_register("lang");
if(isset($_SESSION["lang"]) && !is_string($_SESSION["lang"])){
	unset($lang);
}
if(isset($_SESSION["lang"])){
	$lang = $_SESSION["lang"];
}

// TODO: Add some web stuffs to change the language manually on the web interface...
// ...HERE...
//

// $lang = "fr_FR";

// The following code keeps the lang in sessions, so it can be changed manually above with lang=XX
// and if there's nothing in session yet, then it will parse the language set in the browser
if(isset($lang)){
	$_SESSION["lang"] = $lang;
}
if (!isset($lang)){
	if (isset($_SERVER["HTTP_ACCEPT_LANGUAGE"]) && $_SERVER["HTTP_ACCEPT_LANGUAGE"]) {
		$all_languages = strtok($_SERVER["HTTP_ACCEPT_LANGUAGE"],";");
		$langaccept = explode(",", $all_languages);
		for ($i = 0; $i < sizeof($langaccept); $i++) {
			$tmplang = trim($langaccept[$i]);
			$tmplang2 = substr($tmplang,0,2);
			if (!isset($lang) && isset($txt_langname[$tmplang]) && $txt_langname[$tmplang]) {
				$lang = $tmplang;
			}elseif (!isset($lang) && isset($txt_langname[$tmplang2])) {
				$lang = $tmplang2;
			}
		}
	}
	if (!isset($lang)) {
		$lang = $txt_default_lang;
	}
	$_SESSION["lang"] = $lang;
}
header("Content-type: text/html; charset=UTF-8");
switch($lang){
case "fr_FR":
case "fr":
	$gettext_lang = "fr_FR.UTF-8";
	break;
default:
	$gettext_lang = "en_US.UTF-8";
	break;
}
if(FALSE === putenv("LC_ALL=$gettext_lang")){
	echo "Failed to putenv LC_ALL=$gettext_lang<br>";
}
if(FALSE === putenv("LANG=$gettext_lang")){
	echo "Failed to putenv LANG=$gettext_lang<br>";
}
if(FALSE === setlocale(LC_ALL, $gettext_lang)){
	echo "Failed to setlocale(LC_ALL,$gettext_lang)<br>";
}
$pathname = bindtextdomain("messages", dirname(__FILE__) ."/translations/locale");
$message_domain = textdomain("messages");

// This comes from the Mail_Mime PEAR package, under Debian, you need
// the php-mail-mime package to have this script work.
require_once 'Mail/mimeDecode.php';

// Email header parsing
function decodeEmail($input){
	$params['include_bodies'] = true;
	$params['decode_bodies']  = true;
	$params['decode_headers'] = true;

	$decoder = new Mail_mimeDecode($input);
	$structure = $decoder->decode($params);
	return $structure;
}

// Reading of all the messages from the moderation folder
function getMessageList(){
	global $mod_dir;
	$all_msg = array();
	// Read all files from the directory.
	if (is_dir($mod_dir)) {
		if ($dh = opendir($mod_dir)) {
			while (($file = readdir($dh)) !== false) {
				$full_path = $mod_dir . "/" . $file;
				if(filetype($full_path) == "file"){
					if(FALSE === ($input = file_get_contents($full_path))){
						echo "<span class=\"errorMessages\">"._("Warning: could not read")." $full_path</span><br>";
					}else{
						$struct = decodeEmail($input);
						$my_msg = array();
						$my_msg["headers"] = $struct->headers;
						$my_msg["body"] = $struct->body;
						$my_msg["filename"] = $file;
						$all_msg[] = $my_msg;
					}
				}
			}
			closedir($dh);
		}
	}
	return $all_msg;
}

// Printing of the list of messages to be displayed (the big table)
function printAllMessages($all_msg){
	$n = sizeof($all_msg);
	$out = "<table cellspacing=\"4\" cellpadding=\"4\" border=\"0\">\n";
	$th = " class=\"tableTitle\" ";
	$out .= "<tr><th$th><input onClick=\"checkscript();\" type=\"submit\" value=\""._("Inverse selection")."\"></th><th$th>"._("Date")."</th><th$th>"._("Subject")."</th><th$th>"._("From")."</th></tr>\n";
	$out .= "<form id=\"zeForm\" name=\"zeForm\" action=\"". $_SERVER["PHP_SELF"] ."\">";
	for($i=0;$i<$n;$i++){
		if($i % 2){
			$cls = " class=\"alternatecolorline\" ";
		}else{
			$cls = " class=\"alternatecolorline2\" ";
		}
		$out .= "<tr>";
		$out .= "<td $cls>" . "<input type=\"checkbox\" name=\"msg_id[]\" value=\"". $all_msg[$i]["filename"] ."\">" . "</td>";
		$out .= "<td $cls>" . htmlspecialchars($all_msg[$i]["headers"]["date"]) . "</td>";
		if( strlen($all_msg[$i]["headers"]["subject"]) == 0){
			$subject = _("No subject");
		}else{
			$subject = $all_msg[$i]["headers"]["subject"];
		}
		$out .= "<td $cls><a href=\"". $_SERVER["PHP_SELF"] ."?action=show_message&msgid=". $all_msg[$i]["filename"] ."\">" . htmlspecialchars($subject) . "</a></td>";
		$out .= "<td $cls>" . htmlspecialchars($all_msg[$i]["headers"]["from"]) . "</td>";
		$out .= "</tr>\n";
	}
	$out .= "</table>\n";
	$out .= _("For the selection:")." ";
	$out .= "<input type=\"submit\" name=\"validate\" value=\""._("Validate")."\">";
	$out .= "<input type=\"submit\" name=\"delete\" value=\""._("Delete")."\">";
	return $out;
}

// Deletion and validation of messages
if( isset($_REQUEST["validate"]) || isset($_REQUEST["delete"])){
	if( !isset($_REQUEST["msg_id"]) ){
		echo "<span class=\"errorMessages\">"._("No message selected!")."</span><br>";
	}else{
		$n = sizeof($_REQUEST["msg_id"]);
		for($i=0;$i<$n;$i++){
			if( !ereg("^([0-9a-f]+)\$",$_REQUEST["msg_id"][$i]) ){
				echo "<span class=\"errorMessages\">".("Moderation ID format is wrong: will ignore this one!")."</span>";
				continue;
			}
			$fullpath = $mod_dir . "/" . $_REQUEST["msg_id"][$i];
			if(!file_exists($fullpath)){
				echo "<span class=\"errorMessages\">"._("Moderation ID not found in the moderation folder!")."</span>";
				continue;
			}
			// TODO: Check if message is there!
			if( isset($_REQUEST["validate"]) ){
//				echo "Validating message ".$_REQUEST["msg_id"][$i]."<br>";
				$to_addr = $list_name . $delimiter . "moderate-" . $_REQUEST["msg_id"][$i] . "@" . $list_domain;
				$headers = "From: ".$from_addr;
				mail($to_addr,"MLMMJ Web interface moderation","MLMMJ Web interface moderation",$headers);
			}else{
//				echo "Deleting message ".$_REQUEST["msg_id"][$i]."<br>";
				unlink($mod_dir . "/" . $_REQUEST["msg_id"][$i]);
			}
		}
	}
}

// A bit of javascript to do the checkboxes inversion.
echo "<html>
<head>
<link REL=\"stylesheet\" TYPE=\"text/css\" href=\"mlmmj.css\">
<script language=\"JavaScript\" type=\"text/javascript\">
function getObj(name){
	if (document.getElementById){
		this.obj = document.getElementById(name);
		this.style = document.getElementById(name).style;
	}else if (document.all){
		this.obj = document.all[name];
		this.style = document.all[name].style;
	}else if (document.layers){
		this.obj = document.layers[name];
		this.style = document.layers[name];
	}
}
function checkscript(){
	var frm = document.forms['zeForm'];
	var n = frm.elements.length;
	for(i=0;i<n;i++){
		if(frm.elements[i].type == 'checkbox'){
			if(frm.elements[i].checked == true){
				frm.elements[i].checked = false;
			}else{
				frm.elements[i].checked = true;
			}
		}
	}
}
</script>
</head>
<body>
<h3>MLMMJ "._("moderation web interface")."</h3>
<a href=\"".$_SERVER["PHP_SELF"]."\">"._("Refresh page")."</a>";

if( isset($_REQUEST["action"]) && $_REQUEST["action"] == show_message){
	if( !ereg("^([0-9a-f]+)\$",$_REQUEST["msgid"]) ){
		echo "<span class=\"errorMessages\">"._("Message ID format is wrong: can't display!")."</span>";
	}else{
		$full_path = $mod_dir . "/" . $_REQUEST["msgid"];
		if( !file_exists($full_path) ){
			echo "<span class=\"errorMessages\">".("Message does not exists!")."</span>";
		}else{
			if(FALSE === ($input = file_get_contents($full_path))){
				echo "<span class=\"errorMessages\">"._("Error: could not read")." $full_path </span><br>";
			}else{
				$struct = decodeEmail($input);
				echo "<a href=\"". $_SERVER["PHP_SELF"] ."\">"._("Back to messages list")."</a><br><br>";
				echo "<b>"._("From:")." </b>" . htmlspecialchars($struct->headers["from"])."<br>";
				echo "<b>"._("Date:")." </b>" . htmlspecialchars($struct->headers["date"])."<br>";
				echo "<b>"._("To:")." </b>" . htmlspecialchars($struct->headers["to"])."<br>";
				echo "<b>"._("Subject:")." </b>" . htmlspecialchars($struct->headers["subject"])."<br>";
				echo "<b>"._("Message body:")."</b><br><br>" . nl2br(htmlspecialchars($struct->body));
			}
		}
	}
}else{
	$all_msg = getMessageList();
	echo printAllMessages($all_msg);
}

echo '
<div class="footer">
<a href="http://www.mlmmj.org">MLMMJ</a>: Mailing List Manager Make it Joyfull<br />
<span class="italics">
MLMMJ PHP moderator code done by:
<a target="_blank" href="http://thomas.goirand.fr">Thomas GOIRAND</a>, released under the therms of the
MIT license. Please visit GPLHost\'s leading
<a target="_blank" href="http://www.gplhost.com/hosting-vps.html">VPS hosting</a> service, and
<a target="_blank" href="http://www.gplhost.com/software-dtc.html">DTC GPL control panel home</a>
if you want hosting with MLMMJ support.
</span>
</div>
</body></html>';

?>
<?php

/* Copyright (C) 2004 Christoph Thiel <ct at kki dot org>
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


// error_reporting(E_ALL);

class mlmmj
{
    var $email;
    var $mailinglist;
    var $job;
    var $redirect_success;
    var $redirect_failure;

    var $delimiter;
    var $errors;

    function is_email($string="") 
	{
	    if (eregi("^[a-z0-9\._-]+".chr(64)."+[a-z0-9\._-]+\.+[a-z]{2,4}$", $string)) 
	    { 
		return TRUE; 
	    }
	    else 
	    { 
		return FALSE; 
	    }
	}

    function error($string="")
	{
	    $this->errors = TRUE;
//	    die($string);
	}

    function mlmmj()
	{
	    // set mandatory vars...
	    $this->errors = FALSE;
	    $this->delimiter = "+";

	    if (!isset($_POST["email"]) &&
		!isset($_POST["mailinglist"]) &&
		!isset($_POST["job"]) &&
		!isset($_POST["redirect_success"]) &&
		!isset($_POST["redirect_failure"]))
	    {
		$this->errors = TRUE;
		if(isset($_POST["redirect_failure"]))
		{
		    header("Location: ".$_POST["redirect_failure"]);
		    exit;
		}
		else
		    die("An error occurred. Please check contrib/web/php-user/README for details.");
	    }
	    else
	    {
		if($this->is_email($_POST["email"]))
		    $this->email = $_POST["email"];
		else
		    $this->error("ERROR: email is not a valid email address.");

		if($this->is_email($_POST["mailinglist"]))
		    $this->mailinglist = $_POST["mailinglist"];
		else
		    $this->error("ERROR: mailinglist is not a valid email address.");
		
		$this->job = $_POST["job"];
		
		if(!(($this->job == "subscribe") OR ($this->job == "unsubscribe")))
		{
		    $this->error("ERROR: job unknown.");
		}
		
		$this->redirect_failure = $_POST["redirect_failure"];
		$this->redirect_success = $_POST["redirect_success"];

	    }

	    // now we should try to go ahead and {sub,unsub}scribe... ;)

	    if(!$this->errors)
	    {
		// @ ^= char(64)
		
		$to = str_replace(chr(64),$this->delimiter.$this->job.chr(64),$this->mailinglist);
		$subject = $this->job." to ".$this->mailinglist;
		$body = $this->job;
		$addheader = "";
		$addheader .= "Received: from ". $_SERVER["REMOTE_ADDR"]
		    ." by ". $_SERVER["SERVER_NAME"]. " with HTTP;\r\n\t".date("r")."\n";
		$addheader .= "X-Originating-IP: ".$_SERVER["REMOTE_ADDR"]."\n";
		$addheader .= "X-Mailer: mlmmj-webinterface powered by PHP/". phpversion() ."\n";
		$addheader .= "From: ".$this->email."\n";
		$addheader .= "Cc: ".$this->email."\n";
		
		if(!mail($to, $subject, $body, $addheader))
		    $this->error($this->job." failed.");
	    }

	    if($this->errors)
	    {
		header("Location: ".$this->redirect_failure);
		exit;
	    }
	    else
	    {
		header("Location: ".$this->redirect_success);
		exit;
	    }
	}
}


$mailinglist = new mlmmj;

?>

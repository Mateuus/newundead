<?php
	require_once('dbinfo.inc.php');

	$email       = urldecode($_REQUEST['email']);
	$completeurl = urldecode($_REQUEST['completeurl']);
	if(strlen($email) < 1) die('email');
	if(strlen($completeurl) < 1) die('completeurl');

	$tsql   = "exec [BreezeNet].dbo.BN_WarZ_NewsletterSubscribe ?";
	$params = array($email);
	$member = db_exec($conn, $tsql, $params);

	header("Location: $completeurl");
?>
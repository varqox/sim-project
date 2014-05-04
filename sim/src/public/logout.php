<?php
require_once $_SERVER['DOCUMENT_ROOT']."/../php/session.php";
if(isset($_COOKIE['session']))
{
	session::start();
	setcookie('session', null, -1, '/');
	session::destroy();
}
header('Location: /');
?>
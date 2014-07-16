<?php
require_once 'session.php';

function check_logged_in()
{
	if(!isset($_COOKIE['session']))
		return false;
	session::start();
	if(!isset($_SESSION) || $_SESSION['user_agent_ip'] != $_SERVER['HTTP_USER_AGENT'].$_SERVER['REMOTE_ADDR'])
	{
		setcookie('session', null, -1, '/');
		unset($_COOKIE['session']);
		session::write_close();
		return false;
	}
	// session::write_close();
	return true;
}

function template_begin($title, $scripts='', $styles='')
{
echo '<!DOCTYPE html>
<html lang="en">
<head>
<meta charset="utf-8">
<title>',$title,'</title>
<link rel="stylesheet" href="/kit/styles.css">
<script src="/kit/jquery.js"></script>
<script src="/kit/scripts.js"></script>
<script> var start_time=', round(microtime(true) * 1000), ', load=-1, time_difference;</script>
<link rel="shortcut icon" href="/kit/img/favicon.png">',(strlen($scripts)==1 ? '':"\n<script>".$scripts.'</script>'),(strlen($styles)==0 ? '':"\n<style>".$styles.'</style>'),'
</head>
<body onload="updateClock()">
<div class="navbar">
<div class="navbar-body">
<a href="/" class="brand">SIM</a>
<a href="/round.php">Contests</a>
<a href="/files/">Files</a>
<a href="/submissions/">Submissions</a>
<div style="float:right">
<span id="clock"></span>';
if(check_logged_in())
{
	echo '<div class="dropdown">
<a href="#" class="user"><strong>',$_SESSION['username'],'</strong><b class="caret"></b></a>
<ul>
<li><a href="/logout.php">logout</a></li>
</ul>
</div>';
}
else
	echo '<a href="/login.php"><strong>Log in</strong></a>
<a href="/register.php">Register</a>';
	echo '</div>
</div>
</div>
<div class="body">
';
}

function template_end()
{
echo '
</div>
</body>
</html>';
}

function E_404()
{
	header('HTTP/1.0 404 Not Found');
	template_begin('404 Not Found');
	echo "<center><h1 style=\"font-size:25px;font-weight:normal;\">404 &mdash; Page not found</h1></center>";
	template_end();
	exit;
}
function E_403()
{
	header('HTTP/1.0 403 Forbidden');
	template_begin('403 Forbidden');
	echo "<center><h1 style=\"font-size:25px;font-weight:normal;\">403 &mdash; Sorry, but you're not allowed to see anything here.</h1></center>";
	template_end();
	exit;
}
function privileges($x)
{
	if($x == 'admin')
		return 1;
	if($x == 'teacher')
		return 2;
	return 3;
}
?>

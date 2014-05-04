<?php
require_once 'session.php';

function check_loged_in()
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
<script src="/kit/scripts.js"></script>
<script> var start_time=', round(microtime(true) * 1000), ', load=-1, time_difference;</script>
<link rel="shortcut icon" href="/kit/img/favicon.png">',(strlen($scripts)==0 ? '':"\n<script>".$scripts.'</script>'),(strlen($styles)==0 ? '':"\n<style>".$styles.'</style>'),'
</head>
<body onload="updateClock()">
<div class="navbar">
<div class="navbar-body">
<a href="/" class="brand">SIM</a>
<a href="/contests.php">Konkursy</a>
<a href="/admin.php">Admin</a>
<a href="/files/">Files</a>
<a href="/reports/">Reports</a>
<div class="dropdown">
<span id="clock"></span>';
if(check_loged_in())
{
	echo '<a onclick="f(this);" class="user"><strong>',$_SESSION['username'],'</strong><b class="caret"></b></a>
	<ul>
	<li><a href="/logout.php">logout</a></li>
	<li><a href="/bodzio.php">bodzio</a></li>
	<li><a href="/add_report.php">Submit a solution</a></li>
	<li><a href="/test.php">test</a></li>
	<li><a href="/trol.php">troll</a></li>
	</ul>';
}
else
	echo '<a href="/login.php" class="user"><strong>Log in</strong></a>';
echo '</div>
</div>
</div>
<div class="body-main">
';
}

function template_end()
{
echo '

</div>
</body>
</html>';
}
?>
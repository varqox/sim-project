<?php
require_once $_SERVER['DOCUMENT_ROOT']."/../php/main.php";

if($_SERVER['REQUEST_METHOD'] == 'POST' && isset($_POST['username'], $_POST['password']))
{
	$stmt = DB::pdo()->prepare("SELECT username, first_name, last_name, type FROM users WHERE username = :username AND password = :password");
	$stmt->bindValue(':username', $_POST['username'], PDO::PARAM_STR);
	$stmt->bindValue(':password', hash('sha256', $_POST['password']), PDO::PARAM_STR);
	$stmt->execute();
	if($row = $stmt->fetch())
	{
		session::start();
		$_SESSION['username'] = $row['username'];
		$_SESSION['first_name'] = $row['first_name'];
		$_SESSION['last_name'] = $row['last_name'];
		$_SESSION['type'] = $row['type'];
		$_SESSION['user_agent_ip'] = $_SERVER['HTTP_USER_AGENT'].$_SERVER['REMOTE_ADDR'];
	}
	$stmt->closeCursor();
}
if(check_loged_in())
{
	// echo "<p>Yeah! You're logged in ", $_SESSION['first_name'], " ", $_SESSION['last_name'], " (", $_SESSION['username'], ")</p>";
	header('Location: /');
}
else
{
	template_begin('Login page');
	echo '<form method="post"  style="margin-left: auto;margin-right: auto">
<input type="text" name="username"/>
<input type="password" name="password"/>
<input type="submit" value="Log in"/>
</form>';
	template_end();
}
?>
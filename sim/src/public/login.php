<?php
require_once $_SERVER['DOCUMENT_ROOT']."/../php/main.php";

if($_SERVER['REQUEST_METHOD'] == 'POST' && isset($_POST['username'], $_POST['password']))
{
	$stmt = DB::pdo()->prepare("SELECT id,username,first_name,last_name FROM users WHERE username = ? AND password = ?");
	$stmt->bindValue(1, $_POST['username']);
	$stmt->bindValue(2, hash('sha256', $_POST['password']));
	$stmt->execute();
	if($row = $stmt->fetch())
	{
		session::start();
		$_SESSION['id'] = $row['id'];
		$_SESSION['username'] = $row['username'];
		$_SESSION['first_name'] = $row['first_name'];
		$_SESSION['last_name'] = $row['last_name'];
		$_SESSION['user_agent_ip'] = $_SERVER['HTTP_USER_AGENT'].$_SERVER['REMOTE_ADDR'];
	}
	$stmt->closeCursor();
}
if(check_logged_in())
	header('Location: /');
else
{
	template_begin('Login page');
	echo '<div class="form-container">
<h1>Log in</h1>
<form method="post">
<label>Username</label>
<input class="input-block" type="text" name="username"/>
<label>Password</label>
<input class="input-block" type="password" name="password"/>
<input type="submit" value="Log in"/>
</form>
</div>';
	template_end();
}
?>

<?php
require_once $_SERVER['DOCUMENT_ROOT']."/../php/main.php";

global $info;

if($_SERVER['REQUEST_METHOD'] == 'POST')
{
	if(!isset($_POST['username'], $_POST['first_name'], $_POST['last_name'], $_POST['email'], $_POST['password1'], $_POST['password2']))
	{
		$info.="<p>Wrong form was sent</p>";
		goto form;
	}
	if($_POST['username'] == '')
	{
		$info.="<p>Login can't be blank</p>";
		goto form;
	}
	if($_POST['first_name'] == '')
	{
		$info.="<p>First name can't be blank</p>";
		goto form;
	}
	if($_POST['last_name'] == '')
	{
		$info.="<p>Last name can't be blank</p>";
		goto form;
	}
	if($_POST['password1'] != $_POST['password2'])
	{
		$info.="<p>Passwords don't match</p>";
		goto form;
	}
	if(!preg_match("\"^[_a-z0-9-]+(\.[_a-z0-9-]+)*@[a-z0-9-]+(\.[a-z0-9-]+)*(\.[a-z]{2,4})$\"", $_POST['email']))
	{
		$info.="<p>Wrong email</p>";
		goto form;
	}
	$info.="<pre>";
	// print_r($_POST);
	$stmt = DB::pdo()->prepare("INSERT INTO users (username, first_name, last_name, email, password) SELECT ?, ?, ?, ?, ? FROM users WHERE NOT EXISTS (SELECT id FROM users WHERE username=?) LIMIT 1");
	$stmt->bindValue(1, $_POST['username'], PDO::PARAM_STR);
	$stmt->bindValue(2, $_POST['first_name'], PDO::PARAM_STR);
	$stmt->bindValue(3, $_POST['last_name'], PDO::PARAM_STR);
	$stmt->bindValue(4, $_POST['email'], PDO::PARAM_STR);
	$stmt->bindValue(5, hash('sha256', $_POST['password1']), PDO::PARAM_STR);
	$stmt->bindValue(6, $_POST['username'], PDO::PARAM_STR);
	$stmt->execute();
	if(1 > $stmt->rowCount())
	{
		$info.="<p>Username taken</p></pre>";
		goto form;
	}

	session::start();

	$stmt = DB::pdo()->prepare("SELECT id FROM users WHERE username = ?");
	$stmt->bindValue(1, $_POST['username'], PDO::PARAM_STR);
	$stmt->execute();
	if($row = $stmt->fetch())
	{
		$_SESSION['id'] = $row[0];
		$stmt->closeCursor();
	}

	$_SESSION['username'] = $_POST['username'];
	$_SESSION['first_name'] = $_POST['first_name'];
	$_SESSION['last_name'] = $_POST['last_name'];
	$_SESSION['user_agent_ip'] = $_SERVER['HTTP_USER_AGENT'].$_SERVER['REMOTE_ADDR'];
	$info.="<p>Sometime later write it...</p></pre>";
}
if(check_logged_in())
{
	template_begin('Register page');
	echo $info,"<p>Yeah! You're logged in ", $_SESSION['first_name'], " ", $_SESSION['last_name'], " (", $_SESSION['username'], ")</p>";
}
else
{
form:
	template_begin('Register page');
	echo $info,'<div class="form-container">
<h1>Register</h1>
<form method="post">
<label>Username</label>
<input class="input-block" type="text" value="',isset($_POST['username']) ? $_POST['username'] : '','" name="username"/>
<label>First name</label>
<input class="input-block" type="text" value="',isset($_POST['first_name']) ? $_POST['first_name'] : '','" name="first_name"/>
<label>Last name</label>
<input class="input-block" type="text" value="',isset($_POST['last_name']) ? $_POST['last_name'] : '','" name="last_name"/>
<label>Email</label>
<input class="input-block" type="text" value="',isset($_POST['email']) ? $_POST['email'] : '','" name="email"/>
<label>Password</label>
<input class="input-block" type="password" name="password1"/>
<label>Repeat password</label>
<input class="input-block" type="password" name="password2"/>
<input type="submit" value="Register"/>
</form>
</div>
';
}
template_end();
?>
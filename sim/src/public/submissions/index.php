<?php
require_once $_SERVER['DOCUMENT_ROOT']."/../php/main.php";

if(!isset($_GET['id']) || !is_numeric($_GET['id']))
	$_GET['id']=1;

if(!check_logged_in())
{
	header("Location: /login.php");
	exit;
}

$stmt = DB::pdo()->prepare("SELECT type FROM users WHERE id=?");
$stmt->bindValue(1, $_SESSION['id']);
$stmt->execute();
if(!($row = $stmt->fetch()))
	E_403();
$user_privileges = privileges($row[0]);
$stmt->closeCursor();

$stmt = DB::pdo()->prepare("SELECT name,parent,privileges,begin_time,author FROM rounds WHERE id=?");
$stmt->bindValue(1, $_GET['id']);
$stmt->execute();
if(1 > $stmt->rowCount())
	E_404();
$time = time();
$row = $stmt->fetch();
$path = "<a href=\"/round.php?id=".$_GET['id']."\">$row[0]</a>";
$parent = $row[1];
if(privileges($row[2]) < $user_privileges)
	E_403();
if(isset($row[3]) && $_SESSION['id'] != $row[4] && privileges($row[2]) == $user_privileges && time() < strtotime($row[3]))
	E_403();
$stmt->closeCursor();

$stmt = DB::pdo()->prepare("SELECT id,type FROM users WHERE id=?");
$stmt->bindValue(1, $row[4]);
$stmt->execute();
$admin_view = $show_admin_view = false;
if(0 < $stmt->rowCount()) {
	$row = $stmt->fetch();
	if($_SESSION['id'] == $row[0] || privileges($row[1]) > $user_privileges) {
		$show_admin_view = true;
		if(isset($_GET['admin']))
			$admin_view = true;
	}
	$stmt->closeCursor();
}

$stmt = DB::pdo()->prepare("SELECT name,parent,privileges FROM rounds WHERE id=?");
while($parent > 1)
{
	$stmt->bindValue(1, $parent);
	$stmt->execute();
	$row = $stmt->fetch();
	$path = "<a href=\"/round.php?id=$parent\">$row[0]</a> / $path";
	$parent = $row[1];
	if(privileges($row[2]) < $user_privileges)
		E_403();
	$stmt->closeCursor();
}

template_begin('My Submissions','','.body{margin-left:180px}');

echo '<ul class="menu"><li><a href="/round.php?id=',$_GET['id'],'">View round</a></li>',($admin_view ? '<li><a href="?id='.$_GET['id'].'">User view</a></li>' : ($show_admin_view ? '<li><a href="?id='.$_GET['id'].'&admin">Admin view</a></li>' : '')),'</ul>';

echo '<div class="path">',$path," / Submissions</div>";
if($admin_view)
{
	$stmt = DB::pdo()->prepare("SELECT sr.submission_id, sr.time, t.name, s.status, s.points, r.full_judge_time, u.username FROM submissions_to_rounds sr, submissions s, tasks t, rounds r, users u WHERE sr.round_id=? AND sr.user_id=u.id AND sr.submission_id=s.id AND s.round_id=r.id AND s.task_id=t.id ORDER BY sr.submission_id DESC");
	$stmt->bindValue(1,$_GET['id']);
	$stmt->execute();
	if(1 > $stmt->rowCount())
		echo "<center>Noone have submitted anything yet...</center>";
	else
	{
		echo '<table class="submissions">
<thead>
<tr>
<th style="min-width: 60px">User</th>
<th style="min-width: 150px">Submission time</th>
<th style="min-width: 120px">Problem</th>
<th style="min-width: 200px">Status</th>
<th style="min-width: 60px">Score</th>
</tr>
</thead>
<tbody>
';
		while($row = $stmt->fetch())
		{
			echo '<tr>
<td>',$row[6],'</td>
<td><a href="/submissions/',$row[0],'.php">',$row[1],'</a></td>
<td>',$row[2],'</td>
<td';
		switch($row[3])
		{
			case "ok": echo ' class="ok">Initial tests: OK';break;
			case "error": echo ' class="wa">Initial tests: Error';break;
			case "c_error": echo ' class="wa">Compilation failed';break;
			case "waiting": echo '>Pending';break;
		}
		echo '</td>
<td>',$row[4],'</td>
</tr>
';
		}
		echo '</tbody>
</table>';
	}
}
else
{
	$stmt = DB::pdo()->prepare("SELECT sr.submission_id, sr.time, t.name, s.status, s.points, r.full_judge_time FROM submissions_to_rounds sr, submissions s, tasks t, rounds r WHERE sr.round_id=? AND sr.user_id=? AND sr.submission_id=s.id AND s.round_id=r.id AND s.task_id=t.id ORDER BY sr.submission_id DESC");
	$stmt->bindValue(1,$_GET['id']);
	$stmt->bindValue(2,$_SESSION['id']);
	$stmt->execute();
	if(1 > $stmt->rowCount())
		echo "<center>You have not submitted anything yet...</center>";
	else
	{
		echo '<table class="submissions">
<thead>
<tr>
<th style="min-width: 150px">Submission time</th>
<th style="min-width: 120px">Problem</th>
<th style="min-width: 200px">Status</th>
<th style="min-width: 60px">Score</th>
</tr>
</thead>
<tbody>
';
		while($row = $stmt->fetch())
		{
			echo '<tr>
<td><a href="/submissions/',$row[0],'.php">',$row[1],'</a></td>
<td>',$row[2],'</td>
<td';
		switch($row[3])
		{
			case "ok": echo ' class="ok">Initial tests: OK';break;
			case "error": echo ' class="wa">Initial tests: Error';break;
			case "c_error": echo ' class="wa">Compilation failed';break;
			case "waiting": echo '>Pending';break;
		}
		echo '</td>
<td>',($row[5] == NULL || strtotime($row[5]) <= time() ? $row[4] : ''),'</td>
</tr>
';
		}
		echo '</tbody>
</table>';
	}
}
template_end();
?>

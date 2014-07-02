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
$stmt->bindValue(1, $_SESSION['id'], PDO::PARAM_STR);
$stmt->execute();
if(!($row = $stmt->fetch()))
	E_403();
$user_privileges = privileges($row[0]);
$stmt->closeCursor();

$stmt = DB::pdo()->prepare("SELECT name,parent,privileges,begin_time,author FROM rounds WHERE id=?");
$stmt->bindValue(1, $_GET['id'], PDO::PARAM_INT);
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

$stmt = DB::pdo()->prepare("SELECT name,parent,privileges FROM rounds WHERE id=?");
while($parent > 1)
{
	$stmt->bindValue(1, $parent, PDO::PARAM_INT);
	$stmt->execute();
	$row = $stmt->fetch();
	$path = "<a href=\"/round.php?id=$parent\">$row[0]</a> / $path";
	$parent = $row[1];
	if(privileges($row[2]) < $user_privileges)
		E_403();
	$stmt->closeCursor();
}

template_begin('My Submissions');
echo '<div class="round-info">',$path," / Submissions</div>";
$stmt = DB::pdo()->prepare("SELECT rr.report_id, rr.time, t.name, r.status, r.points FROM reports_to_rounds rr, reports r, tasks t WHERE rr.round_id=? AND rr.user_id=? AND rr.report_id=r.id AND r.task_id=t.id ORDER BY rr.report_id DESC");
$stmt->bindValue(1,$_GET['id'], PDO::PARAM_STR);
$stmt->bindValue(2,$_SESSION['id'], PDO::PARAM_STR);
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
<td ';
	switch($row[3])
	{
		case "ok":
			echo 'class="ok">OK';
			break;
		case "error":
			echo 'class="wa">Error';
			break;
		case "c_error":
			echo 'class="wa">Compilation failed';
			break;
		case "waiting":
			echo '>Pending';
			break;
	}
	echo '</td>
<td>',$row[4],'</td>
</tr>
';
	}
	echo '</tbody>
</table>';
}
template_end();
?>
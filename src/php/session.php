<?php
require_once 'db.php';

define('session_maxlifetime', 60*60*24);

class session
{
	private static $obj;
	private $name, $id;

	static private function generate_id($length)
	{
		static $dict = 'abcdefghijklmnopqrstuvwxyz0123456789';
		$result = '';
		for($i = 0, $len = strlen($dict) - 1; $i < $length; ++$i)
			$result.=$dict[mt_rand(0, $len)];
		return $result;
	}

	private function __construct($new_name)
	{
		global $_COOKIE;
		global $_SESSION;
		$this->name = $new_name;
		if(isset($_COOKIE[$this->name]))
			$this->id = $_COOKIE[$this->name];
		else
		{
			self::clean_up();
			while(0 >= DB::pdo()->exec("INSERT INTO session (id, data) VALUES ('".($this->id = self::generate_id(30))."', '[]')"));
			setcookie($this->name, $this->id, 0, "/");
			$_COOKIE[$this->name] = $this->id;
		}
		$stmt = DB::pdo()->query("SELECT s.* FROM session s WHERE s.id = '".$this->id."'");
		if($row = $stmt->fetch())
		{
			if(time() - strtotime($row['time']) > session_maxlifetime)
			{
				$stmt->closeCursor();
				self::clean_up();
				while(0 >= DB::pdo()->exec("INSERT INTO session (id, data) VALUES ('".($this->id = self::generate_id(30))."', '[]')"));
				setcookie($this->name, $this->id, 0, "/");
				$_COOKIE[$this->name] = $this->id;
				return;
			}
			$_SESSION = json_decode($row['data'], true);
		}
		$stmt->closeCursor();
	}

	private function __clone(){}

	function __destruct()
	{
		$stmt = DB::pdo()->prepare("INSERT INTO session (id, data, time) VALUES (?, ?, ?) ON DUPLICATE KEY UPDATE data=VALUES(data), time=VALUES(time)");
		$stmt->bindValue(1, $this->id, PDO::PARAM_STR);
		$stmt->bindValue(2, json_encode($_SESSION), PDO::PARAM_STR);
		$stmt->bindValue(3, date("Y-m-d H:i:s"), PDO::PARAM_STR);
		$stmt->execute();
	}

	static private function clean_up()
	{
		try
		{
			DB::pdo()->exec("DELETE FROM session WHERE time < '".date("Y-m-d H:i:s", time() - session_maxlifetime)."'");
		}
		catch(PDOExeption $e)
		{
		}
	}

	static public function start($name = 'session')
	{
		if(!isset(self::$obj))
			self::$obj = new session($name);
	}

	static public function id($new_id = '')
	{
		if(func_num_args() == 0)
			return self::$obj->id;
		self::$obj->id = $new_id;
		setcookie(self::$obj->name, $new_id, 0, "/");
		$_COOKIE[self::$obj->name] = $new_id;
		return $new_id;
	}

	static public function regenerate_id()
	{
		DB::pdo()->exec("DELETE FROM session WHERE id = '".self::$obj->id."'");
		while(0 >= DB::pdo()->exec("INSERT INTO session (id, data) VALUES ('".(self::$obj->id = self::generate_id(30))."', '[]')"));
		setcookie(self::$obj->name, self::$obj->id, 0, "/");
		$_COOKIE[self::$obj->name] = self::$obj->id;
	}

	static public function write_close()
	{
		$tmp = self::$obj;
		unset($tmp);
		self::$obj = null;
	}

	static public function destroy()
	{
		$tmp = self::$obj;
		$id = $tmp->id;
		unset($tmp);
		self::$obj = null;
		DB::pdo()->exec("DELETE FROM session WHERE id = '".$id."'");
	}
}

?>
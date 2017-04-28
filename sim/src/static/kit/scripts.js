function hex2str(hexx) {
	var hex = hexx.toString(); //force conversion
	var str = '';
	for (var i = 0; i < hex.length; i += 2)
		str += String.fromCharCode(parseInt(hex.substr(i, 2), 16));
	return str;
}

// Clock
$(document).ready(function updateClock() {
	if (updateClock.time_difference === undefined)
		updateClock.time_difference = window.performance.timing.responseStart - start_time;

	var time = new Date();
	time.setTime(time.getTime() - updateClock.time_difference);
	var hours = time.getHours();
	var minutes = time.getMinutes();
	var seconds = time.getSeconds();
	hours = (hours < 10 ? '0' : '') + hours;
	minutes = (minutes < 10 ? '0' : '') + minutes;
	seconds = (seconds < 10 ? '0' : '') + seconds;
	// Update the displayed time
	var tzo = -time.getTimezoneOffset();
	document.getElementById('clock').innerHTML = String().concat(hours, ':', minutes, ':', seconds, '<sup>UTC', (tzo >= 0 ? '+' : ''), tzo / 60, '</sup>');
	setTimeout(updateClock, 1000 - time.getMilliseconds());
});

// Dropdowns
$(document).ready(function(){
	$(document).click(function(event) {
		if ($(event.target).is('.dropmenu .dropmenu-toggle, .dropmenu .dropmenu-toggle *')) {
			var dropmenu = $(event.target).closest('.dropmenu');
			if (dropmenu.is('.open'))
				dropmenu.removeClass('open');
			else {
				$('.dropmenu.open').removeClass('open');
				dropmenu.addClass('open');
			}
		} else
			$('.dropmenu.open').removeClass('open');
	});
});

// Converts datetimes to local
function normalize_datetime(elem, add_tz) {
	elem = $(elem);
	var time;
	if (isNaN(elem.attr('datetime'))) {
		var args = elem.attr('datetime').split(/[- :]/);
		--args[1]; // fit month in [0, 11]
		time = new Date(Date.UTC.apply(this, args));
	} else
		time = new Date(elem.attr('datetime') * 1000);

	var month = time.getMonth() + 1;
	var day = time.getDate();
	var hours = time.getHours();
	var minutes = time.getMinutes();
	var seconds = time.getSeconds();
	month = (month < 10 ? '0' : '') + month;
	day = (day < 10 ? '0' : '') + day;
	hours = (hours < 10 ? '0' : '') + hours;
	minutes = (minutes < 10 ? '0' : '') + minutes;
	seconds = (seconds < 10 ? '0' : '') + seconds;

	// Add the timezone part
	if (add_tz) {
		var tzo = -(new Date()).getTimezoneOffset();
		elem.html(String().concat(time.getFullYear(), '-', month, '-', day,
			' ', hours, ':', minutes, ':', seconds,
			'<sup>UTC', (tzo >= 0 ? '+' : ''), tzo / 60, '</sup>'));
	} else
		elem.html(String().concat(time.getFullYear(), '-', month, '-', day,
			' ', hours, ':', minutes, ':', seconds));

	return elem;
};

$(document).ready(function() {
	// Converts datetimes
	var tzo = -(new Date()).getTimezoneOffset();
	$('*[datetime]').each(function() {
		normalize_datetime($(this),
			$(this).parents('.submissions, .problems, .jobs, .files').length == 0);
	});

	// Give timezone info in the submissions and problems table
	$('.submissions th.time, .problems th.added, .jobs th.added, .files th.time') .each(function() {
		$(this).children().remove();
		$(this).append(String().concat(
			'<sup>UTC', (tzo >= 0 ? '+' : ''), tzo / 60, '</sup>'));
	});
});
// Handle navbar correct size
function normalizeNavbar() {
	var navbar = $('.navbar');
	navbar.css('width', 'auto');

	if (navbar.outerWidth() <= $(window).width()) {
		navbar.css('width', '100%');
		navbar.css('position', 'fixed');
	} else {
		navbar.css('position', 'absolute');
		navbar.outerWidth($(document).width());
	}
}
$(document).ready(normalizeNavbar);
$(window).resize(normalizeNavbar);

// Handle menu correct size
function normalizeMenu() {
	var menu = $('.menu');
	menu.css('height', 'auto');

	if (menu.outerHeight() <= $(window).height()) {
		menu.css('height', '100%');
		menu.css('position', 'fixed');
	} else {
		menu.css('position', 'absolute');
		menu.outerHeight($(document).height());
	}
}
$(document).ready(normalizeMenu);
$(window).resize(normalizeMenu);

// Returns value of cookie @p name or ... TODO!!!
function getCookie(name) {
	name = name + '=';
	var arr = document.cookie.split(';');
	for (i = 0; i < arr.length; ++i) {
		var j = 0;
		while (arr[i].charAt(j) == ' ')
			++j;
		if (arr[i].indexOf(name, j) == j)
			return arr[i].substring(j + name.length, arr[i].length);
	}

	return '';
};

// Adding csrf token to a form
function addCsrfTokenTo(form) {
	$(form).find('input[name="csrf_token"]').remove(); // Avoid duplication
	form.append('<input type="hidden" name="csrf_token" value="' +
		getCookie('csrf_token') + '">');
}

// Adding csrf token just before submitting a form
$(function() {
	$('form').submit(function() { addCsrfTokenTo($(this)); });
});

// Modal
$(document).click(function(event) {
	if ($(event.target).is('.modal'))
		$(event.target).remove();
	else if ($(event.target).is('.modal .close'))
		$(event.target).parent().parent().remove();
});
function modal(modal_body) {
	$('<div>', {
		class: 'modal',
		html: $('<div>', {
			html: $('<span>', { class: 'close' }).add(modal_body)
		})
	}).appendTo('body');
}
function modalForm(form_title, form_body) {
	modal($('<h2>', {
		text: form_title
	}).add('<form>', {
		// enctype: 'application/x-www-form-urlencoded',
		style: 'display: flex; flex-flow: column nowrap',
		html: form_body
	}));
}
function sendModalFrom(url, success_msg) {
	var form = $('.modal form');
	addCsrfTokenTo(form);
	$('#modal_req_status').remove();
	$('<p>', {
		id: 'modal_req_status',
		text: 'Sending request...'
	}).appendTo(form);
	console.log(form.serialize());
	$.ajax({
		type: 'POST',
		url: url,
		data: form.serialize(),
		success: function() {
			$('#modal_req_status').html(success_msg + ' <a onclick="location.reload(true)">Reload the page</a> to see changes.')
				.css('background', '#9ff55f');
		},
		error: function(resp) {
			$('#modal_req_status').replaceWith($('<pre>', {
				id: 'modal_req_status',
				text: "Error: " + resp.status + ' ' + resp.statusText,
				style: 'background: #f55f5f'
			}));
			// Additional message
			try {
				var xml = $.parseXML(resp.responseText);
				var msg = $(xml).text();

				if (msg != '') {
					var x = $('#modal_req_status');
					x.text(x.text().concat("\nInfo: ", msg));
				}
			} catch (err) {
				if (resp.responseText != '' // There is a message
					&& resp.responseText.lastIndexOf('<!DOCTYPE html>', 0)
						!== 0 // Message is not a whole HTML page
					&& resp.responseText.lastIndexOf('<!doctype html>', 0)
						!== 0) // Message is not a whole HTML page
				{
					var x = $('#modal_req_status');
					x.text(x.text().concat("\nInfo: ",
						resp.responseText));
				}
			}

		}
	});
	return false;
}
function modalFormSubmitButton(value, url, success_msg, css_classes, cancel_button_message)
{
	// Default arguments
	if (css_classes === undefined)
		css_classes = 'blue';
	if (cancel_button_message === undefined)
		cancel_button_message = null;

	return $('<div>', {
		style: 'margin: 8px auto 0',
		html: $('<input>', {
			type: 'submit',
			class: 'btn-small ' + css_classes,
			value: value,
			click: function() {
				return sendModalFrom(url, success_msg);
			}
		}).add((cancel_button_message === null ? '' : $('<input>', {
			type: 'button',
			class: 'btn-small',
			value: cancel_button_message,
			click: function() {
				$(this).parent().parent().parent().parent().remove();
			}
		})))
	});
}
function changeSubmissionType(submission_id, stype) {
	modalForm('Change submission type',
		$('<div>', {
			html: $('<label>', {
				text: 'New submission type: '
			}).add('<select>', {
				name: 'stype',
				html: $('<option>', {
					value: 'n/f',
					text: 'Normal / final',
					selected: (stype === 'n/f')
				}).add('<option>', {
					value: 'i',
					text: 'Ignored',
					selected: (stype === 'i')
				})
			})
		}).add(modalFormSubmitButton('Change type',
			'/s/' + submission_id + '/change-type',
			'Submission type has been changed.'))
	);
}
function rejudgeSubmission(submission_id) {
	modalForm('Rejudge submission ' + submission_id);
	sendModalFrom('/s/' + submission_id + '/rejudge',
		'Rejudge of the submission was added to the job queue.')
}
function rejudgeProblemSubmissions(problem_id) {
	modalForm('Rejudge submissions to the problem ' + problem_id,
		$('<div>', {
			html: $('<label>', {
				html: 'Are you sure to rejudge all the submissions to the problem <a href="/p/' + problem_id +
					'">' + problem_id + '</a>?',
			})
		}).add(modalFormSubmitButton('Rejudge submissions',
			'/p/' + problem_id + '/rejudge',
			'Rejudge of the submissions was added to the job queue', 'blue',
			'No, go back'))
	);
}
function rejudgeRoundSubmissions(round_id) {
	modalForm('Rejudge submissions in the round',
		$('<div>', {
			html: $('<label>', {
				html: 'Are you sure to rejudge all the submissions in the round <a href="/c/' + round_id +
					'">' + round_id + '</a>?',
			})
		}).add(modalFormSubmitButton('Rejudge submissions',
			'/c/' + round_id + '/edit/rejudge',
			'Rejudge of the submissions was added to the job queue', 'blue',
			'No, go back'))
	);
}

function addContestUser(contest_id) {
	modalForm('Add user to the contest',
		$('<div>', {
			class: 'field-group',
			style: 'margin-top: 5px',
			html: $('<div>', {
				html: $('<select>', {
				name: 'user_value_type',
					html: $('<option>', {
						value: 'username',
						text: 'Username',
						selected: true
					}).add('<option>', {
						value: 'uid',
						text: 'Uid'
					})
				})
			}).add('<input>', {
				type: 'text',
				name: 'user'
			})
		}).add('<div>', {
			class: 'field-group',
			html: $('<label>', {
				text: 'User mode'
			}).add('<select>', {
				name: 'mode',
				html: $('<option>', {
					value: 'c',
					text: 'Contestant',
					selected: true
				}).add('<option>', {
					value: 'm',
					text: 'Moderator'
				})
			})
		}).add(modalFormSubmitButton('Add user',
			'/c/' + contest_id + '/users/add',
			'User has been added.'))
	);
}
function changeContestUserMode(contest_id, user_id, mode) {
	modalForm('Change user mode',
		$('<div>', {
			html: $('<label>', {
				text: "New user's mode: "
			}).add('<select>', {
				name: 'mode',
				html: $('<option>', {
					value: 'c',
					text: 'Contestant',
					selected: (mode === 'c')
				}).add('<option>', {
					value: 'm',
					text: 'Moderator',
					selected: (mode === 'm')
				})
			}).add('<input>', {
				type: 'hidden',
				name: 'uid',
				value: user_id
			})
		}).add(modalFormSubmitButton('Change mode',
			'/c/' + contest_id + '/users/change-mode',
			'User mode has been changed.'))
	);
}
function expelContestUser(contest_id, user_id, username) {
	modalForm('Expel user from the contest',
		$('<div>', {
			html: $('<label>', {
				html: 'Are you sure to expel the user <a href="/u/' + user_id +
					'">' + username + '</a> with the id = ' + user_id + '?',
			}).add('<input>', {
				type: 'hidden',
				name: 'uid',
				value: user_id
			})
		}).add(modalFormSubmitButton('Of course!',
			'/c/' + contest_id + '/users/expel',
			'User has been expelled.', 'red',
			'No, the user may stay'))
	);
}
function cancelJob(job_id) {
	modalForm('Cancel job ' + job_id);
	sendModalFrom('/jobs/' + job_id + '/cancel',
		'The job has been canceled.');
}
function restartJob(job_id) {
	modalForm('Restart job',
		$('<div>', {
			html: $('<label>', {
				html: 'Are you sure to restart the <a href="/jobs/' + job_id +
					'">job ' + job_id + '</a>?',
			})
		}).add(modalFormSubmitButton('Restart job',
			'/jobs/' + job_id + '/restart',
			'The job has been restarted.', 'orange',
			'No, go back'))
	);
}

function colorize(log, end) {
	if (end > log.length)
		end = log.length;

	var res = "";
	var opened = null;

	function close_last_tag() {
		if (opened == 'span')
			res += '</span>';
		else if (opened == 'b')
			res += '</b>';
		opened = null;
	};

	function contains(idx, str) {
		return (log.substring(idx, idx + str.length) == str);
	};
	for (var i = 0; i < end; ++i) {
		if (contains(i, '\033[31m')) {
			close_last_tag();
			res += '<span class="red">';
			opened = 'span';
			i += 4;
		} else if (contains(i, '\033[32m')) {
			close_last_tag();
			res += '<span class="green">';
			opened = 'span';
			i += 4;
		} else if (contains(i, '\033[33m')) {
			close_last_tag();
			res += '<span class="yellow">';
			opened = 'span';
			i += 4;
		} else if (contains(i, '\033[34m')) {
			close_last_tag();
			res += '<span class="blue">';
			opened = 'span';
			i += 4;
		} else if (contains(i, '\033[35m')) {
			close_last_tag();
			res += '<span class="magentapink">';
			opened = 'span';
			i += 4;
		} else if (contains(i, '\033[36m')) {
			close_last_tag();
			res += '<span class="turquoise">';
			opened = 'span';
			i += 4;
		} else if (contains(i, '\033[1;31m')) {
			close_last_tag();
			res += '<b class="red">';
			opened = 'b';
			i += 6;
		} else if (contains(i, '\033[1;32m')) {
			close_last_tag();
			res += '<b class="green">';
			opened = 'b';
			i += 6;
		} else if (contains(i, '\033[1;33m')) {
			close_last_tag();
			res += '<b class="yellow">';
			opened = 'b';
			i += 6;
		} else if (contains(i, '\033[1;34m')) {
			close_last_tag();
			res += '<b class="blue">';
			opened = 'b';
			i += 6;
		} else if (contains(i, '\033[1;35m')) {
			close_last_tag();
			res += '<b class="pink">';
			opened = 'b';
			i += 6;
		} else if (contains(i, '\033[1;36m')) {
			close_last_tag();
			res += '<b class="turquoise">';
			opened = 'b';
			i += 6;
		} else if (contains(i, '\033[m') && opened !== null) {
			close_last_tag();
			opened = null;
			i += 2;
		} else
			res += log[i];
	}
	close_last_tag();

	return res + log.substring(end);
}

function Logs(type, elem) {
	this.type = type;
	this.elem = $(elem);
	this.offset = undefined;
	this.lock = false;
	this.fetch_more = function() {
		if (this.lock)
			return;
		this.lock = true;

		$(this.elem).children('.loader, .loader-info').remove();
		this.elem.append('<img class="loader" src="/kit/img/loader.gif">');
		var logs = this;
		$.ajax({
			type: 'GET',
			url: '/api/logs/' + logs.type +
				(logs.offset === undefined ? '' : '?' + logs.offset),
			success: function(data) {
				data = String(data).split('\n');
				logs.offset = data[0];
				data = hex2str(data[1]);

				var prev_height = elem[0].scrollHeight;
				var prev = prev_height - elem.scrollTop();

				logs.elem.children('.loader').remove();
				elem.html(colorize($('<div/>').text(data).html() + elem.html(),
					data.length + 2000));
				var curr_height = elem[0].scrollHeight;
				elem.scrollTop(curr_height - prev);

				// Load more logs if scrolling up did not become possible
				logs.lock = false;
				if (logs.offset > 0 && (elem.innerHeight() >= curr_height || prev_height == curr_height))
					logs.fetch_more();
			},
			error: function(resp) {
				logs.elem.children('.loader').remove();
				logs.elem.append($('<span>', {
					class: 'loader-info',
					html: $('<span>', {
						text: "Error: " + resp.status + ' ' + resp.statusText
					}).add('<a>', {
						text: 'Try again',
						click: function () { new Logs(logs.type, logs.elem); }
					})
				}));

				// Additional message
				var x = logs.elem.children('.loader > span');
				try {
					var xml = $.parseXML(resp.responseText);
					var msg = $(xml).text();

					if (msg != '')
						x.text(x.text().concat("\nInfo: ", msg));

				} catch (err) {
					if (resp.responseText != '' // There is a message
						&& resp.responseText.lastIndexOf('<!DOCTYPE html>', 0)
							!== 0 // Message is not a whole HTML page
						&& resp.responseText.lastIndexOf('<!doctype html>', 0)
							!== 0) // Message is not a whole HTML page
					{
						x.text(x.text().concat("\nInfo: ",
							resp.responseText));
					}
				}

				logs.lock = false;
			}
		});
	};

	this.monitor_scroll = function() {
		var logs = this;
		this.elem.on('scroll', function() {
			if (logs.offset != 0 && $(this).scrollTop() <= 300)
				logs.fetch_more();
		});
	};

	this.fetch_more();
}

function Jobs(base_url, elem) {
	this.base_url = base_url;
	this.elem = $(elem);
	this.lock = false;
	this.fetch_more = function() {
		if (this.lock)
			return;
		this.lock = true;

		this.elem.parent().children('.loader, .loader-info').remove();
		this.elem.parent().append('<img class="loader" src="/kit/img/loader.gif">');

		function getid(url) { return url.substr(url.lastIndexOf('/') + 1); }

		var lower_job_id = this.elem.find("tr:last-child td:nth-child(2)");
		var query = (lower_job_id.length === 0 ? ''
			: '/<' + getid(lower_job_id.children().attr('href')));

		var jobs = this;
		$.ajax({
			type: 'GET',
			url: jobs.base_url + query,
			dataType: 'json',
			success: function(data) {
				for (x in data) {
					x = data[x];
					var row = $('<tr>');
					row.append($('<td>', {text: x[2]}));
					row.append($('<td>', {
						html: normalize_datetime($('<a>', {
							datetime: x[1],
							href: '/jobs/' + x[0],
							text: x[1]
						}), false)
					}));
					row.append($('<td>', {
						class: 'status ' + x[3][0],
						text: x[3][1]
					}));
					row.append($('<td>', {
						html: x[5] === null ? 'System' : $('<a>', {
							href: '/u/' + x[5],
							text: x[6]
						})
					}));
					row.append($('<td>', {text: x[4]}));
					// Info
					var info = x[7];
					row.append(function() {
						var td = $('<td>');
						function append_tag(name, val) {
							td.append($('<label>', {text: name}));
							td.append(val);
						}

						if (info.submission !== undefined)
							append_tag('submission', $('<a>', {
								href: '/s/' + info.submission,
								text: info.submission
							}))

						if (info.problem !== undefined)
							append_tag('problem', $('<a>', {
								href: '/p/' + info.problem,
								text: info.problem
							}))

						var names = ['name', 'memory limit', 'make public'];
						for (var idx in names) {
							var x = names[idx];
							if (info[x] !== undefined)
								append_tag(x, info[x]);
						}

						return td;
					}());

					// Actions
					var td = $('<td>');
					td.append($('<a>', {
						class: 'btn-small',
						href: '/jobs/' + x[0],
						text: 'View job'
					}));
					for (var idx in x[8]) {
						var c = x[8][idx];

						if (c == 'P')
							td.append($('<div>', {
								class: 'dropmenu down',
								html: $('<a>', {
									class: 'btn-small dropmenu-toggle',
									text: 'Download'
								}).add($('<ul>', {
									html: $('<a>', {
										href: '/jobs/' + x[0] + '/report',
										text: 'Report'
									}).add($('<a>', {
										href: '/jobs/' + x[0] +
											'/download-uploaded-package',
										text: 'Uploaded package'
									}))
								}))
							}));

						if (c == 'V')
							td.append($('<a>', {
								class: 'btn-small green',
								href: '/p/' + info.problem,
								text: 'View problem'
							}));

						if (c == 'c')
							td.append($('<a>', {
								class: 'btn-small red',
								text: 'Cancel job',
								click: function() {
									var job_id = x[0];
									return function() { cancelJob(job_id); }
								}()
							}));

						if (c == 'r')
							td.append($('<a>', {
								class: 'btn-small orange',
								text: 'Restart job',
								click: function() {
									var job_id = x[0];
									return function() { restartJob(job_id); }
								}()
							}));
					}

					row.append(td);
					jobs.elem.children('tbody').append(row);
				}

				jobs.elem.parent().children('.loader').remove();

				// Load more jobs if scrolling up did not become possible
				if (data.length > 0)
					jobs.lock = false;

				if ($(document).height() - $(window).height() <= 300)
					jobs.fetch_more();
			},
			error: function(resp) {
				jobs.elem.parent().children('.loader').remove();
				jobs.elem.parent().append($('<span>', {
					class: 'loader-info',
					html: $('<span>', {
						text: "Error: " + resp.status + ' ' + resp.statusText
					}).add('<a>', {
						text: 'Try again',
						click: function () { new Jobs(jobs.base_url, jobs.elem); }
					})
				}));

				// Additional message
				var x = jobs.elem.parent().children('.loader > span');
				try {
					var xml = $.parseXML(resp.responseText);
					var msg = $(xml).text();

					if (msg != '')
						x.text(x.text().concat("\nInfo: ", msg));

				} catch (err) {
					if (resp.responseText != '' // There is a message
						&& resp.responseText.lastIndexOf('<!DOCTYPE html>', 0)
							!== 0 // Message is not a whole HTML page
						&& resp.responseText.lastIndexOf('<!doctype html>', 0)
							!== 0) // Message is not a whole HTML page
					{
						x.text(x.text().concat("\nInfo: ",
							resp.responseText));
					}
				}

				jobs.lock = false;
			}
		});
	};

	this.monitor_scroll = function() {
		var jobs = this;
		$(window).on('scroll resize', function() {
			if ($(document).height() - $(window).height() - $(document).scrollTop() <= 300)
				jobs.fetch_more();
		});
	};

	this.fetch_more();
}

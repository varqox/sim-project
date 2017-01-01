// Clock
window.onload = function updateClock() {
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
}

// Dropdowns
$(document).ready(function(){
	$('.dropmenu > .dropmenu-toggle').click(function(event) {
		event.preventDefault();
		if($(this).parent().is('.open'))
			$(this).parent().removeClass('open');
		else {
			$('.dropmenu.open').removeClass('open');
			$(this).parent().addClass('open');
		}
	});
	$(document).click(function(event) {
		if(!$(event.target).is('.dropmenu-toggle, .dropmenu-toggle *'))
			$('.dropmenu.open').removeClass('open');
	});
});

// Converts datetimes to local
$(document).ready(function() {
	// Converts datetimes
	var tzo = -(new Date()).getTimezoneOffset();
	$('*[datetime]').each(function() {
		var x = $(this), time;
		if (isNaN(x.attr('datetime'))) {
			var args = x.attr('datetime').split(/[- :]/);
			--args[1]; // fit month in [0, 11]
			time = new Date(Date.UTC.apply(this, args));
		} else
			time = new Date(x.attr('datetime') * 1000);

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

		// If this is a '.submissions .time', '.problems .added' or
		// '.jobs .added', then skip the timezone part
		if (x.parents('.submissions, .problems, .jobs').length)
			x.html(String().concat(time.getFullYear(), '-', month, '-', day,
				' ', hours, ':', minutes, ':', seconds));
		else
			x.html(String().concat(time.getFullYear(), '-', month, '-', day,
				' ', hours, ':', minutes, ':', seconds,
				'<sup>UTC', (tzo >= 0 ? '+' : ''), tzo / 60, '</sup>'));
	});

	// Give timezone info in the submissions and problems table
	$('.submissions th.time, .problems th.added, .jobs th.added').each(function() {
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
				addCsrfTokenTo($(this).parent());
				$('#modal_req_status').remove();
				$('<p>', {
					id: 'modal_req_status',
					text: 'Sending request...'
				}).appendTo($(this).parent().parent());
				$.ajax({
					type: 'POST',
					url: url,
					data: $(this).parent().parent().serialize(),
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
	modalForm('Cancel job',
		$('<div>', {
			html: $('<label>', {
				html: 'Are you sure to cancel the <a href="/jobs/' + job_id +
					'">job ' + job_id + '</a>?',
			})
		}).add(modalFormSubmitButton('Cancel job',
			'/jobs/' + job_id + '/cancel',
			'The job has been canceled.', 'red',
			'No, go back'))
	);
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

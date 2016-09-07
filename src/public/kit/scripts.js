window.onload = function updateClock() {
	if (typeof updateClock.time_difference === 'undefined')
		updateClock.time_difference = window.performance.timing.responseStart - start_time;

	var time = new Date();
	time.setTime(time.getTime() - updateClock.time_difference);
	var hours = time.getUTCHours();
	var minutes = time.getUTCMinutes();
	var seconds = time.getUTCSeconds();
	hours = (hours < 10 ? '0' : '') + hours;
	minutes = (minutes < 10 ? '0' : '') + minutes;
	seconds = (seconds < 10 ? '0' : '') + seconds;
	// Update the displayed time
	document.getElementById('clock').innerHTML = String().concat(hours, ':', minutes, ':', seconds, ' UTC');
	setTimeout(updateClock, 1000 - time.getMilliseconds());
}
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
// Returns value of cookie @p name or ... TODO!!!
function getCookie(name) {
	name = name + '=';
	console.log(name);
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
// Adding csrf token before submitting a form
$(function() {
	$('form').submit(function() { addCsrfTokenTo($(this)); });
});
// Modal
function changeSubmissionType(submission_id, stype) {
	$('<div>', {
		class: 'modal',
		html: $('<div>', {
			html: $('<span>', { class: 'close' })
			.add('<h2>', {
				text: 'Change submission type'
			}).add('<form>', {
				style: 'display: flex; flex-flow: column nowrap',
				html: $('<div>', {
					html: $('<span>', {
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
				}).add('<input>', {
					type: 'submit',
					class: 'btn-small blue',
					style: 'margin: 8px auto 0',
					value: 'Change',
					click: function() {
						addCsrfTokenTo($(this).parent());
						$('#req_status').remove();
						$('<p>', {
							id: 'req_status',
							text: 'Sending request...'
						}).appendTo($(this).parent());
						$.ajax({
							type: 'POST',
							url: '/s/' + submission_id + '/change-type',
							data: $(this).parent().serialize(),
							success: function() {
								$('#req_status').text("Submission type changed. Reload the page to see changes.")
									.css('background', '#9ff55f');
							},
							error: function(req) {
								$('#req_status').text("Error: " + req.status
									+ ' ' + req.statusText)
									.css('background', '#f55f5f');
							}
						});
						return false;
					}
				})
			})
		})
	}).appendTo('body');
}
$(document).click(function(event) {
	if ($(event.target).is('.modal'))
		$(event.target).remove();
	else if ($(event.target).is('.modal .close'))
		$(event.target).parent().parent().remove();
});

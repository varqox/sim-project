# Assumptions

## Rounds
- Begin_time <= min(begin_times of subrounds)
- End_time >= min(end_times of subrounds)
- Max name length is 128 characters
- You have normal access to round if you have greater or equal privileges (if equal round had to start) to rounds on path from current to root round
- You have superuser access (editing round) if you fulfil at least one point of:
 - You have normal access and grater privileges than author of this round
 - You are author of this round
- Problem in round is also a round

## Submissions
- You have normal access if you are author of submission
- You have superuser access if you have superuser access to round (to which submission belongs)

## Problems
- Max name length is 128 characters

## Checkers
- Max name length is 32 characters

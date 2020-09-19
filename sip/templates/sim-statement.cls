\NeedsTeXFormat{LaTeX2e}
\ProvidesClass{sim-statement}[2020/09/20 Sim problem statement]
\def\@baseclass{article}
% Language can be either en or pl
\newif\ifmyclass@en
\DeclareOption{en}{\myclass@entrue}
\DeclareOption{pl}{\myclass@enfalse}
\ExecuteOptions{en}
\DeclareOption*{\PassOptionsToClass{\CurrentOption}{\@baseclass}}
\ProcessOptions\relax
\LoadClass{\@baseclass}

\ifmyclass@en
    \RequirePackage[english]{babel}
\else
    \RequirePackage[polish]{babel}
\fi
\RequirePackage[a4paper, total={480pt, 720pt}]{geometry}
\RequirePackage[lastpage,user]{zref}
\RequirePackage[T1]{fontenc}
\RequirePackage[utf8]{inputenc}
\RequirePackage{array}
\RequirePackage{booktabs}
\RequirePackage{calc}
\RequirePackage{enumitem}
\RequirePackage{fancyhdr}
\RequirePackage{fancyvrb}
\RequirePackage{float}
\RequirePackage{graphicx}
\RequirePackage{hyperref}
\RequirePackage{longtable}
\RequirePackage{scalerel}
\RequirePackage{tabularx}
\RequirePackage{xparse}

\renewcommand{\arraystretch}{1.25}
\hypersetup{colorlinks=true, urlcolor=blue}

\let\oldverbatim\verbatim
\let\oldendverbatim\endverbatim
\def\verbatim{\Verbatim[samepage=true]}
\def\endverbatim{\endVerbatim}

\let\OldIncludegraphics\includegraphics
\renewcommand{\includegraphics}[2][]{\OldIncludegraphics[scale=0.55, #1]{#2}}

\providecommand{\tightlist}{%
    \setlength{\itemsep}{0pt}\setlength{\parskip}{0pt}}

\setcounter{secnumdepth}{0}

%%%%%%%%%%%%%%%%%%%%%%%%%%%%% Problem description %%%%%%%%%%%%%%%%%%%%%%%%%%%%%

\def\problemName#1{\def\@problemName{#1}}
\def\memoryLimit#1{\def\@memoryLimit{#1}}
\def\additionalFooterInfo#1{\def\@additionalFooterInfo{#1}}
\def\contestName#1{\def\@contestName{#1}}
\def\date#1{\def\@date{#1}}

\problemName{}
\memoryLimit{}
\additionalFooterInfo{}
\contestName{}
\date{}

\pagestyle{fancy}
\fancyhf{}
\renewcommand{\headrulewidth}{0pt}
\renewcommand{\footrulewidth}{0.5pt}
\lfoot{\@additionalFooterInfo}
\cfoot{\thepage/\zpageref{LastPage}}
\rfoot{\@problemName}

\newenvironment{statement}
{
    \section*
    {\huge\@problemName\hfill\small{\strut}
        \ifx \@memoryLimit \@empty \else
            \small{\ifmyclass@en Available memory\else Dostępna pamięć\fi: \@memoryLimit}
        \fi
        \newline
        \rule{480pt}{0.5pt}
        \ifx \@contestName \@empty \else
            \small\@contestName
        \fi
        \ifx \@date \@empty \else
            \strut\hfill\normalfont\textit\@date
        \fi
    }

    \setlength{\parindent}{0pt}
    \setlength{\parskip}{0.4\baselineskip}

    \begin{sloppypar}
}
{
    \end{sloppypar}
}

\def\InputWord{\ifmyclass@en Input\else Wejście\fi}
\def\OutputWord{\ifmyclass@en Output\else Wyjście\fi}

\NewDocumentCommand{\sectionInput}{}{\section{\InputWord}}

\NewDocumentCommand{\sectionOutput}{}{\section{\OutputWord}}

%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%% Sample tests %%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%

\ExplSyntaxOn
\NewDocumentCommand{\readcell}{m}
{
    \sip_statement_readcell:n { #1 } % #1 is the file to read
    \vspace{-\baselineskip} % remove empty line that results from the trailing \newline
}
% some variables
\tl_new:N \l__sip_statement_cell_content_tl
\ior_new:N \g_sip_statement_import_stream
% proper implementation
\cs_new_protected:Npn \sip_statement_readcell:n #1
{
    % clear the variable containing the cel contents
    \tl_clear:N \l__sip_statement_cell_content_tl
    % start reading the file
    \ior_open:Nn \g_sip_statement_import_stream { #1 }
    % at each line ...
    \ior_map_inline:Nn \g_sip_statement_import_stream
    {
        % add the line and a trailing \newline
        \tl_put_right:Nn \l__sip_statement_cell_content_tl { ##1 \newline }
    }
    \tl_use:N \l__sip_statement_cell_content_tl % deliver the result
}
\ExplSyntaxOff

\NewDocumentCommand{\testFiles}{mm}{
    \readcell{#1} & \readcell{#2} \\ \hline
}

\NewDocumentCommand{\standardTestFiles}{m}{
    \testFiles{in/#1.in}{out/#1.out}
}

\newenvironment{samples}
{
    \section{\ifmyclass@en Examples\else Przykłady\fi}
    \tabularx{\textwidth}{*{2}{|>{\ttfamily}X}|}
    \hline
        \multicolumn{1}{|c|}{\textbf{\InputWord}} & \multicolumn{1}{c|}{\textbf{\OutputWord}}
    \\ \hline
}
{
    \endtabularx
}

%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%% Scoring %%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%

\newcounter{subtask}\NewDocumentCommand{\subtask}{mm}{
    \stepcounter{subtask}
    \thesubtask & #1 & #2 \\ \hline
}
\def\noRestrictions{\ifmyclass@en no additional constraints\else brak dodatkowych ograniczeń\fi}

\newenvironment{grading}
{
    \section{\ifmyclass@en Grading\else Ocenianie\fi}
    \setcounter{subtask}{0}
    \center
        \tabularx{0.8\textwidth}{|c|X|c|}
            \hline
                \multicolumn{1}{|c|}{\textbf{\ifmyclass@en Subtask\else Podzadanie\fi}} &
                \multicolumn{1}{c|}{\textbf{\ifmyclass@en Constraints\else Ograniczenia\fi}} &
                \multicolumn{1}{c|}{\textbf{\ifmyclass@en Points\else Punkty\fi}}
            \\ \hline
}
{
        \endtabularx
    \endcenter
}

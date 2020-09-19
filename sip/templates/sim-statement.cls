\NeedsTeXFormat{LaTeX2e}
\ProvidesClass{sim-statement}[2020/09/20 Sim problem statement]
\def\@baseclass{article}
\DeclareOption*{\PassOptionsToClass{\CurrentOption}{\@baseclass}}
\ProcessOptions\relax
\LoadClass{\@baseclass}

\RequirePackage[polish]{babel}
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
            \small{Dostępna pamięć: \@memoryLimit}
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

\NewDocumentCommand{\sectionInput}{}
{
    \section{Wejście}
}

\NewDocumentCommand{\sectionOutput}{}
{
    \section{Wyjście}
}

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
    \section{Przykłady}
    \tabularx{\textwidth}{*{2}{|>{\ttfamily}X}|}
    \hline
        \multicolumn{1}{|c|}{\textbf{Wejście}} & \multicolumn{1}{c|}{\textbf{Wyjście}}
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
\def\noRestrictions{brak dodatkowych ograniczeń}

\newenvironment{grading}
{
    \section{Ocenianie}
    \setcounter{subtask}{0}
    \center
        \tabularx{0.8\textwidth}{|c|X|c|}
            \hline
                \multicolumn{1}{|c|}{\textbf{Podzadanie}} & \multicolumn{1}{c|}{\textbf{Ograniczenia}} & \multicolumn{1}{c|}{\textbf{Punkty}}
            \\ \hline
}
{
        \endtabularx
    \endcenter
}

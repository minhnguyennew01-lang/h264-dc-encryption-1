
\NeedsTeXFormat{LaTeX2e}
\newcommand{\classname}{Thesis Template}
\ProvidesClass{\classname}[2022/09/05 v1.0 LaTexTemplate]
\providecommand{\baseclass}{article}
\RequirePackage{etoolbox}
\RequirePackage{xparse}
\newbool{nolistspace}
\newbool{chapteroneline}
\newbool{listtoc}
\newbool{toctoc}
\newbool{parskip}
\newbool{hyperrefsupport}
\booltrue{hyperrefsupport}
\newbool{headsepline}
\newbool{consistentlayout}
\LoadClass{\baseclass}
% Simple interface for the user  to customize the chapter titles 

%----------------------------------------------------------------------------------------
%	CLASS OPTIONS
%----------------------------------------------------------------------------------------
\renewcommand{\baselinestretch}{1.2} % Giãn dòng 1.2
\setlength{\parskip}{3pt} % Spacing after
\setlength{\parindent}{1cm} % Set khoảng cách thụt đầu dòng mỗi đoạn
\usepackage{titlesec} % Thư viện để set up các kiểu chữ
\setcounter{secnumdepth}{4} % 4 Heading
\titlespacing*{\section}{0pt}{0pt}{30pt} % Heading 1
\titleformat*{\section}{\fontsize{13pt}{0pt}\selectfont \bfseries \centering}

\titlespacing*{\subsection}{0pt}{6pt}{0pt} % Heading 2
\titleformat*{\subsection}{\fontsize{13pt}{0pt}\selectfont \bfseries}

\titlespacing*{\subsubsection}{0pt}{6pt}{0pt} % Heading 3
\titleformat*{\subsubsection}{\fontsize{13pt}{0pt}\selectfont \bfseries \itshape}

\titlespacing*{\paragraph}{0pt}{0pt}{0pt} % Heading 4
\titleformat*{\paragraph}{\fontsize{13pt}{0pt}\selectfont \itshape}
\usepackage[font=it]{caption}
\renewcommand{\figurename}{\itshape Hình}
\renewcommand{\thefigure}{\thesection.\arabic{figure}.}
\usepackage[font=it]{caption}
\captionsetup[figure]{labelsep=space}

\renewcommand{\tablename}{ \itshape Bảng}
\renewcommand{\thetable}{\thesection.\arabic{table}.}
\usepackage[font=it]{caption}
\captionsetup[table]{labelsep=space}

\usepackage{tabularx}
\newcolumntype{s}{>{\hsize=.3\hsize}X}
\newcolumntype{y}{>{\hsize=.4\hsize}X}
\newcolumntype{d}{>{\hsize=.1\hsize}X}
\newcolumntype{a}{>{\hsize=1.1\hsize}X}
\newcolumntype{g}{>{\hsize=5\hsize}X}
\renewcommand{\tabularxcolumn}[1]{>{\small}m{#1}}

\renewcommand{\theequation}{\thesection.\arabic{equation}} % Thay đổi đánh số phương trình mặc định
%\newtheorem{theorem}{Định lý}[section]
%\newtheorem{defn}[theorem]{Định nghĩa}
%\newtheorem{corollary}[theorem]{Hệ quả}
%\newtheorem{lemma}[theorem]{Bổ đề}
%----------------------------------------------------------------------------------------
%	REQUIRED PACKAGES
%----------------------------------------------------------------------------------------
\RequirePackage{babel} % Required for automatically changing names of document elements to languages besides english
\RequirePackage{scrbase} % Required for handling language-dependent names of sections/document elements
\RequirePackage{scrhack} % Loads fixes for various packages
\RequirePackage{setspace} % Required for changing line spacing
\RequirePackage{longtable} % Required for tables that span multiple pages (used in the symbols, abbreviations and physical constants pages)
\RequirePackage{siunitx} % Required for \SI commands
\RequirePackage{graphicx} % Required to include images
\graphicspath{{Figures/}{./}} % Specifies where to look for included images
%\RequirePackage{booktabs} % Required for better table rules
\RequirePackage{caption} % Required for customising the captions

%----------------------------------------------------------------------------------------
\RequirePackage{geometry}
\geometry{
	paper=a4paper, % Change to letterpaper for US letter
	inner=3cm, % Inner margin
	outer=2cm, % Outer margin
	bindingoffset=.5cm, % Binding offset
	top=2cm, % Top margin
	bottom=2cm, % Bottom margin
	twoside,
	footnotesep=24pt plus 2pt minus 12pt
}
	%showframe, % Uncomment to show how the type block is set on the page
	
\raggedbottom

%----------------------------------------------------------------------------------------
%	PENALTIES
%----------------------------------------------------------------------------------------
\doublehyphendemerits=10000 % No consecutive line hyphens
\brokenpenalty=10000 % No broken words across columns/pages
\widowpenalty=9999 % Almost no widows at bottom of page
\clubpenalty=9999 % Almost no orphans at top of page
\interfootnotelinepenalty=9999 % Almost never break footnotes

%----------------------------------------------------------------------------------------
%	ABBREVIATIONS PAGE DESIGN
%----------------------------------------------------------------------------------------
\renewcommand{\contentsname}{MỤC LỤC}
\renewcommand{\listfigurename}{DANH MỤC HÌNH VẼ}
\renewcommand{\listtablename}{DANH MỤC BẢNG BIỂU}
\renewcommand{\refname}{TÀI LIỆU THAM KHẢO}

%----------------------------------------------------------------------------------------

\ifbool{hyperrefsupport}{% If the nohyperref class option has not been specified
\AtEndPreamble{\RequirePackage{hyperref}
\hypersetup{pdfpagemode={UseOutlines},
bookmarksopen=true,
bookmarksopenlevel=0,
hypertexnames=false,
colorlinks=true,% Set to false to disable coloring links
citecolor=black,% The color of citations
linkcolor=black,% The color of references to document elements (sections, figures, etc)
urlcolor=black,% The color of hyperlinks (URLs)
pdfstartview={FitV},
unicode,
breaklinks=true,
}

\pdfstringdefDisableCommands{% If there is an explicit linebreak in a section heading (or anything printed to the pdf-bookmarks), it is replaced by a space
	\let\\\space%
}
	}
}

%----------------------------------------------------------------------------------------
\bibliographystyle{IEEEtran}

\endinput
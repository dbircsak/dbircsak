import re
import sys
import glob
from datetime import date
from pathlib import Path

import pandas as pd
import pdfplumber


MONTHS = {
    "january": 1,
    "february": 2,
    "march": 3,
    "april": 4,
    "may": 5,
    "june": 6,
    "july": 7,
    "august": 8,
    "september": 9,
    "october": 10,
    "november": 11,
    "december": 12,
}

DATE_LINE_RE = re.compile(
    r"^(?P<tran_mm>\d{2})/(?P<tran_dd>\d{2})\s+"
    r"(?P<post_mm>\d{2})/(?P<post_dd>\d{2})\s+"
    r"(?P<desc>.+?)\s+"
    r"(?P<ref>\d{4})\s+"
    r"\d{4}\s+"  # skip account last4
    r"(?P<amt>-?[\d,]+\.\d{2})\s*$"
)

STMT_RANGE_RE = re.compile(
    r"\b(?P<smon>[A-Za-z]+)\s+(?P<sday>\d{1,2})\s*-\s*"
    r"(?P<emon>[A-Za-z]+)\s+(?P<eday>\d{1,2}),\s*(?P<eyear>\d{4})\b"
)


def parse_statement_date_range(text: str):
    m = STMT_RANGE_RE.search(text)
    if not m:
        return None

    smon = MONTHS.get(m["smon"].lower())
    emon = MONTHS.get(m["emon"].lower())
    if smon is None or emon is None:
        return None

    sday = int(m["sday"])
    eday = int(m["eday"])
    eyear = int(m["eyear"])

    # Infer start year (statement ranges sometimes cross year boundary, e.g., Dec -> Jan).
    if smon > emon:
        syear = eyear - 1
    else:
        syear = eyear

    return (date(syear, smon, sday), date(eyear, emon, eday))


def infer_year(month: int, start: date, end: date) -> int:
    if start.year == end.year:
        return end.year
    return start.year if month >= start.month else end.year


def money_to_float(value: str) -> float:
    # In these PDFs, credits are represented as negative numbers (e.g., -108.69)
    return float(value.replace(",", ""))


def extract_transactions_from_pdf(pdf_path: Path) -> pd.DataFrame:
    print(f"Opening PDF: {pdf_path}")
    rows = []

    with pdfplumber.open(pdf_path) as pdf:
        header_text = ""
        for page in pdf.pages[:3]:
            header_text += (page.extract_text() or "") + "\n"

        stmt_range = parse_statement_date_range(header_text)
        if not stmt_range:
            raise ValueError(f"Statement dates not found in {pdf_path}")

        stmt_start, stmt_end = stmt_range
        stmt_month = stmt_end.strftime("%Y-%m")

        section = None
        current = None
        in_transactions = False

        for page in pdf.pages:
            lines = (page.extract_text() or "").splitlines()

            for line in lines:
                text = line.strip()
                if not text:
                    continue

                low = text.lower()

                # Enter the transactions block
                if low == "transactions":
                    in_transactions = True
                    continue

                if not in_transactions:
                    continue

                # Stop once we hit interest block (keeps out interest lines)
                if low.startswith("interest charged"):
                    section = None
                    break

                # Section switches
                if low == "payments and other credits":
                    section = "payment"
                    continue
                if low == "purchases and adjustments":
                    section = "purchase"
                    continue

                # Totals end a section
                if low.startswith("total payments"):
                    section = None
                    continue
                if low.startswith("total purchases"):
                    section = None
                    continue

                m = DATE_LINE_RE.match(text)
                if m and section:
                    if current:
                        rows.append(current)

                    post_mm = int(m["post_mm"])
                    post_dd = int(m["post_dd"])
                    tran_mm = int(m["tran_mm"])
                    tran_dd = int(m["tran_dd"])

                    post_year = infer_year(post_mm, stmt_start, stmt_end)
                    tran_year = infer_year(tran_mm, stmt_start, stmt_end)

                    amount = money_to_float(m["amt"])
                    category = "credit" if amount < 0 else section

                    current = {
                        "statement_month": stmt_month,
                        "statement_start": stmt_start.isoformat(),
                        "statement_end": stmt_end.isoformat(),
                        "post_date": date(post_year, post_mm, post_dd).isoformat(),
                        "transaction_date": date(tran_year, tran_mm, tran_dd).isoformat(),
                        "reference": m["ref"],
                        "description": m["desc"].strip(),
                        "amount": round(amount, 2),
                        "category": category,
                        "source_pdf": pdf_path.name,
                    }
                elif current and section and text:
                    # Continuation lines (if any) get appended to description
                    # Avoid appending headings/known noise
                    low2 = text.lower()
                    if low2 not in (
                        "transaction posting reference account",
                        "date date description number number amount total",
                    ):
                        current["description"] += " " + text

        if current:
            rows.append(current)

    return pd.DataFrame(rows)


def expand_inputs(patterns):
    files = []
    for pat in patterns:
        matches = glob.glob(pat)
        files.extend(matches)
    return sorted(set(Path(f) for f in files))


def main():
    patterns = sys.argv[1:] or ["*.pdf"]
    pdfs = expand_inputs(patterns)

    if not pdfs:
        print("No PDF files matched the given pattern(s).")
        print("Patterns provided:")
        for p in patterns:
            print(f"  {p}")
        sys.exit(1)

    frames = []
    for pdf in pdfs:
        try:
            frames.append(extract_transactions_from_pdf(pdf))
        except Exception as e:
            print(f"Error processing {pdf}: {e}")
            sys.exit(1)

    df = pd.concat(frames, ignore_index=True)

    out_csv = Path.cwd() / "bofa_credit_card_transactions.csv"
    df.sort_values(["post_date", "transaction_date"], inplace=True)
    df.to_csv(out_csv, index=False)

    print(f"Processed {len(pdfs)} PDF files")
    print(f"Wrote {len(df)} transactions to {out_csv}")


if __name__ == "__main__":
    main()

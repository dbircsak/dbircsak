import re
import sys
import glob
from datetime import date
from pathlib import Path

import pandas as pd
import pdfplumber


DATE_LINE_RE = re.compile(
    r"^(?P<post_mm>\d{2})/(?P<post_dd>\d{2})\s+"
    r"(?P<tran_mm>\d{2})/(?P<tran_dd>\d{2})\s+"
    r"(?P<ref>\d{4})\s+"
    r"(?P<desc>.+?)\s+"
    r"\$(?P<amt>[\d,]+\.\d{2})(?P<cr>CR)?\s*$"
)

STMT_RANGE_RE = re.compile(
    r"(?P<smm>\d{2})/(?P<sdd>\d{2})/(?P<syy>\d{4})\s*-\s*"
    r"(?P<emm>\d{2})/(?P<edd>\d{2})/(?P<eyy>\d{4})"
)


def parse_statement_date_range(text: str):
    m = STMT_RANGE_RE.search(text)
    if not m:
        return None
    return (
        date(int(m["syy"]), int(m["smm"]), int(m["sdd"])),
        date(int(m["eyy"]), int(m["emm"]), int(m["edd"]))
    )


def infer_year(month: int, start: date, end: date) -> int:
    if start.year == end.year:
        return end.year
    return start.year if month >= start.month else end.year


def money_to_float(value: str, is_credit: bool) -> float:
    amt = float(value.replace(",", ""))
    return -amt if is_credit else amt


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

        for page in pdf.pages:
            lines = (page.extract_text() or "").splitlines()

            for line in lines:
                text = line.strip()
                low = text.lower()

                if low == "payments and other credits":
                    section = "payment"
                    continue
                if low == "purchases and other debits":
                    section = "purchase"
                    continue
                if low.startswith("total this period"):
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

                    amount = money_to_float(m["amt"], m["cr"] is not None)

                    current = {
                        "statement_month": stmt_month,
                        "statement_start": stmt_start.isoformat(),
                        "statement_end": stmt_end.isoformat(),
                        "post_date": date(post_year, post_mm, post_dd).isoformat(),
                        "transaction_date": date(tran_year, tran_mm, tran_dd).isoformat(),
                        "reference": m["ref"],
                        "description": m["desc"].strip(),
                        "amount": round(amount, 2),
                        "category": "credit" if amount < 0 else section,
                        "source_pdf": pdf_path.name,
                    }
                elif current and section and text:
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

    out_csv = Path.cwd() / "fidelity_credit_card_transactions.csv"
    df.sort_values(["post_date", "transaction_date"], inplace=True)
    df.to_csv(out_csv, index=False)

    print(f"Processed {len(pdfs)} PDF files")
    print(f"Wrote {len(df)} transactions to {out_csv}")


if __name__ == "__main__":
    main()

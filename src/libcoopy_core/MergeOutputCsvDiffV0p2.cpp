
/*

  Old diff version, kept around out of a policy of supporting old versions.
  This file is frozen, and will only be updated to match gross API changes.

 */

#include <coopy/MergeOutputCsvDiff.h>

#include <coopy/SheetStyle.h>
#include <coopy/DataSheet.h>

#include <stdio.h>
#include <stdlib.h>

#define WANT_MAP2STRING
#define WANT_VECTOR2STRING
#include <coopy/Stringer.h>
#define ROW_COL "ROW"

using namespace std;
using namespace coopy::store;
using namespace coopy::cmp;

MergeOutputCsvDiffV0p2::MergeOutputCsvDiffV0p2() {
  result.setStrict(0);
  result.addField("dtbl",false);
  result.addField("csv",false);
  result.addField("version",false);
  result.addField("0.2",false);
  result.addRecord();
  result.addField("config",false);
  result.addField("empty_tag",false);
  result.addField("*",false);
  result.addRecord();
  result.addField("config",false);
  result.addField("row_tag",false);
  result.addField(ROW_COL,false);
  result.addRecord();
}

bool MergeOutputCsvDiffV0p2::mergeAllDone() {
  SheetStyle style;
  SheetCell c = result.cellSummary(0,0);
  fprintf(out,"%s",result.encode(style).c_str());
  return true;
}

bool MergeOutputCsvDiffV0p2::changeColumn(const OrderChange& change) {
  switch (change.mode) {
  case ORDER_CHANGE_DELETE:
    result.addField("column",false);
    result.addField("delete",false);
    result.addField(ROW_COL,false);
    for (int i=0; i<(int)change.namesAfter.size(); i++) {
      result.addField(change.namesAfter[i].c_str(),false);
    }
    result.addRecord();
    break;
  case ORDER_CHANGE_INSERT:
    result.addField("column",false);
    result.addField("insert",false);
    result.addField(ROW_COL,false);
    for (int i=0; i<(int)change.namesAfter.size(); i++) {
      result.addField(change.namesAfter[i].c_str(),false);
    }
    result.addRecord();
    break;
  case ORDER_CHANGE_MOVE:
    result.addField("column",false);
    result.addField("move",false);
    result.addField(ROW_COL,false);
    for (int i=0; i<(int)change.namesAfter.size(); i++) {
      result.addField(change.namesAfter[i].c_str(),false);
    }
    result.addRecord();
    break;
  default:
    fprintf(stderr,"  Unknown column operation\n\n");
    exit(1);
    break;
  }
  return true;
}

bool MergeOutputCsvDiffV0p2::selectRow(const coopy::cmp::RowChange& change, 
				   const char *tag) {
  result.addField("row",false);
  result.addField(tag,false);
  result.addField("*",false);
  for (int i=0; i<(int)change.names.size(); i++) {
    string name = change.names[i];
    if (change.cond.find(name)!=change.cond.end()) {
      result.addField(change.cond.find(name)->second);
    } else {
      result.addField("*",false);
    }
  }
  result.addRecord();
  return true;
}

bool MergeOutputCsvDiffV0p2::describeRow(const RowChange& change, const char *tag){
  result.addField("row",false);
  result.addField(tag,false);
  result.addField("*",false);
  for (int i=0; i<(int)change.names.size(); i++) {
    string name = change.names[i];
    if (change.val.find(name)!=change.val.end()) {
      result.addField(change.val.find(name)->second);
    } else {
      result.addField("*",false);
    }
  }
  result.addRecord();
  return true;
}

bool MergeOutputCsvDiffV0p2::changeRow(const RowChange& change) {
  switch (change.mode) {
  case ROW_CHANGE_INSERT:
    describeRow(change,"insert");
    break;
  case ROW_CHANGE_DELETE:
    selectRow(change,"delete");
    break;
  case ROW_CHANGE_UPDATE:
    selectRow(change,"select");
    describeRow(change,"update");
    break;
  case ROW_CHANGE_MOVE:
  case ROW_CHANGE_CONTEXT:
    // ignore row ops that were created after v0.2
    break;
  default:
    fprintf(stderr,"  Unknown row operation %s\n\n", change.modeString().c_str());
    exit(1);
    break;
  }
  return true;
}


bool MergeOutputCsvDiffV0p2::declareNames(const std::vector<std::string>& names, 
					  bool final) {
  if (!final) {
    result.addField("column",false);
    result.addField("name",false);
    result.addField(ROW_COL,false);
    for (int i=0; i<(int)names.size(); i++) {
      result.addField(names[i].c_str(),false);
    }
    result.addRecord();
  }
  return true;
}


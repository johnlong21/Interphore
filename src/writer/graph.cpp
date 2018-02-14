#define NODES_MAX 128
#define NODE_NAME_MAX SHORT_STR

namespace Writer {

	void initGraph();
	void submitNode(const char *name, const char *passage);
	void js_submitNode(CScriptVar *v, void *userdata);

	struct Node {
		char name[NODE_NAME_MAX];
		char passage[PASSAGE_NAME_MAX];
	};

	struct GraphStruct {
		Node nodes[NODES_MAX];
		int nodesNum;
	};

	GraphStruct *graph;

	void initGraph() {
		graph = (GraphStruct *)zalloc(sizeof(GraphStruct));
		jsInterp->addNative("function submitNode(name, passage)", &js_submitNode, 0);
	}

	void submitNode(const char *name, const char *passage) {
		Node *node = &graph->nodes[graph->nodesNum++];
		strcpy(node->name, name);
		strcpy(node->passage, passage);
	}

	void js_submitNode(CScriptVar *v, void *userdata) {
		const char *name = v->getParameter("name")->getString().c_str();
		const char *passage = v->getParameter("passage")->getString().c_str();
		submitNode(name, passage);
	}
}

import pygraphviz as pgv
import argparse
import os

# Define the specific order of passes
passes = [
    "0Init",
    "1TopoSort",
    "2ClockOptimize",
    "3RemoveDeadNodes",
    "4ExprOpt",
    "5ConstantAnalysis",
    "6RemoveDeadNodes",
    "7AliasAnalysis",
    "8PatternDetect",
    "9CommonExpr",
    "10RemoveDeadNodes",
    "11MergeNodes",
    "12Replication",
    "13MergeRegister",
    "14ConstructRegs",
    "15Final"
]


def load_dot_file(file_path):
    """Load a DOT file and return the graph."""
    return pgv.AGraph(file_path)


def compare_graphs(graph1, graph2):
    """Compare two graphs and return the differences in nodes and edges."""
    nodes_diff = {
        'added': set(graph2.nodes()) - set(graph1.nodes()),
        'removed': set(graph1.nodes()) - set(graph2.nodes())
    }
    edges_diff = {
        'added': set(graph2.edges()) - set(graph1.edges()),
        'removed': set(graph1.edges()) - set(graph2.edges())
    }
    return nodes_diff, edges_diff


def highlight_differences(graph, nodes_diff, edges_diff):
    """Highlight differences in the graph."""
    for node in nodes_diff['added']:
        graph.get_node(node).attr['color'] = 'green'
        graph.get_node(node).attr['style'] = 'filled'
        graph.get_node(node).attr['fillcolor'] = 'lightgreen'
    for node in nodes_diff['removed']:
        graph.add_node(node, color='red', style='filled',
                       fillcolor='lightcoral')
    for edge in edges_diff['added']:
        u, v = edge
        graph.get_edge(u, v).attr['color'] = 'green'
    for edge in edges_diff['removed']:
        u, v = edge
        graph.add_edge(u, v, color='red', style='dashed')
    return graph


def save_graph(graph, file_path):
    """Save the graph to a DOT file."""
    graph.write(file_path)


def find_dot_files_by_pass(obj_dir, topname, passes):
    """Find dot files matching the specific pass order and topname."""
    files_by_pass = {}
    for p in passes:
        pattern = f"{topname}_{p}.dot"
        for file in os.listdir(obj_dir):
            if file == pattern:
                files_by_pass[p] = os.path.join(obj_dir, file)
                break
    return [files_by_pass[p] for p in passes if p in files_by_pass]


def main(obj_dir, topname):

    files = find_dot_files_by_pass(obj_dir, topname, passes)
    for i in range(len(files) - 1):
        file1 = files[i]
        file2 = files[i + 1]
        output_file = os.path.join(obj_dir, f"{topname}_{passes[i]}_{
                                   passes[i+1]}_diff.dot")

        graph1 = load_dot_file(file1)
        graph2 = load_dot_file(file2)

        print(f'@{i} {files[i]} {files[i + 1]} -> {output_file}')

        nodes_diff, edges_diff = compare_graphs(graph1, graph2)
        highlighted_graph = highlight_differences(
            graph2, nodes_diff, edges_diff)

        save_graph(highlighted_graph, output_file)


if __name__ == "__main__":
    parser = argparse.ArgumentParser(
        description="Automatically compare DOT files according to specific pass order.")
    parser.add_argument("obj_dir", help="Directory containing DOT files.")
    parser.add_argument("topname", help="Top-level name for DOT files.")

    args = parser.parse_args()

    main(args.obj_dir, args.topname)

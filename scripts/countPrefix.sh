
if __name__ == "__main__":
  fp = open("../active-xiangshan-default.txt", "r")
  all_prefix = {}
  all_nodes = {}
  for line in fp.readlines():
    entries = line.split(' ')
    name = entries[1]
    times = entries[4]
    name_prefix = [name]
    while name.find('$') != -1:
      name = name[0:name.rfind('$')]
      name = name.strip('$')
      name_prefix.append(name)
    for prefix in name_prefix:
      if prefix in all_prefix:
        all_prefix[prefix] += int(times)
        all_nodes[prefix] += 1
      else:
        all_prefix[prefix] = int(times)
        all_nodes[prefix] = 1
  sorted_times = sorted(all_prefix.items(), key=lambda x:x[1])
  for entry in sorted_times:
    print(entry, all_nodes[entry[0]], entry[1] / int(all_nodes[entry[0]]))
    # print(all_nodes[entry[0]])
    # print(entry[0] / all_nodes[entry[0]])

        
    